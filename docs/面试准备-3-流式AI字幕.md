# 面试准备：流式AI 字幕

## 一、架构总览

```
┌────────────────┐      ┌─────────────────┐      ┌────────────────┐
│  AudioOutput│      │ AsrRealtime     │      │ SubtitleQueue  │
│  (解码帧)      │─────→│ Strategy│─────→│  (线程安全)    │
│  sendAudio()   │      │  3s/1s 滑窗     │      │  → UI 显示     │
└────────────────┘      └────────┬────────┘      └────────────────┘
                                 │
                   ┌─────────────┴──────────────┐
                   │                             │
            ┌──────▼──────┐            ┌────────▼───────┐
            │  Resampler  │            │   AsrWorker    │
            │ →16kHz mono │            │  whisper_full│
            └──────┬──────┘            └────────────────┘
                   │                             ↑
            ┌──────▼──────────┐         ┌───────┴───────┐
            │ AudioPcmRingBuf │         │ AsrModelCache │
            │  (无锁环形缓冲) │         │ (单例预加载)  │
            └─────────────────┘         └───────────────┘
```

**核心思想**：音频解码帧实时喂入环形缓冲区，独立识别线程以3s 窗口 / 1s步进滑动读取，调用 whisper 识别后做重叠合并去重，推入字幕队列供UI 显示。

---

## 二、策略模式设计

```cpp
class IAsrStrategy {
    virtual bool init(const QString& url, AVStream* audio, SubtitleQueue* queue) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual void sendAudio(AVFrame*) {}// 实时策略需要
    virtual void release() = 0;
    virtual void setModel(const QString& path) = 0;
};
```

| 策略 | 场景 | 特点 |
|------|------|------|
| `AsrRealtimeStrategy` | 播放中实时字幕 | 外部喂帧、环形缓冲、3s/1s 滑窗 |
| `AsrOfflineStrategy` | AI 总结时全量 ASR | 自带Demuxer+Decoder、30s 分段批量识别 |

---

## 三、3s 窗口 / 1s 步进 —— 核心代码

```cpp
void AsrRealtimeStrategy::run() {
    const int SR = 16000;
    const size_t win = 3 * SR;   // 48000 采样点 = 3 秒
    const size_t step = 1 * SR;  // 16000 采样点 = 1 秒
    std::vector<float> buf(win);

    while (running_) {
        if (ring_.available() < win) {
            std::this_thread::sleep_for(2ms);
            continue;
        }
        double start = ring_.head_time_sec();
        ring_.peek(buf.data(), win);     // ← 读3s，不消费

        std::vector<SubtitleItem> res;
        if (worker_->recognize(buf, res, start)) {
            std::string text;
            for (auto& i : res)
                text = AsrUtil::mergeOverlap(text, i.text);
            if (!text.empty() && text != last_text_) {
                last_text_ = text;
                // 推入字幕队列
                queue_->push({text, res.front().start_sec, res.back().end_sec});
            }
        }
        ring_.consume(step);             // ← 只消费 1s → 2s 重叠
    }
}
```

**为什么 3s/1s？**
- 3s：Whisper base 模型在 3s 音频上识别延迟约 150~300ms，是延迟和准确率的平衡点
- 1s步进：相邻两次识别有 2s 重叠，保证句子不会在窗口边界被截断
- 如果用 5s/2s：首字幕延迟太大（5s）；如果 2s/0.5s：短窗口识别准确率下降严重

---

## 四、mergeOverlap —— 最长后缀-前缀匹配合并

```cpp
inline std::string mergeOverlap(const std::string& a, const std::string& b) {
    std::string A = trim(a), B = trim(b);
    if (A.empty()) return B;
    if (B.empty()) return A;
    size_t max = std::min(A.size(), B.size());
    for (size_t l = max; l > 0; l--) {
        if (A.substr(A.size()-l) == B.substr(0, l))
            return A + B.substr(l);   // A的后缀 == B的前缀 →拼接去重
    }
    return B;  // 无重叠 → 直接用新结果
}
```

**例子**：
- A = "今天天气真不错"
- B = "天气真不错啊"
- 重叠 = "天气真不错" (5字)
- 合并 = "今天天气真不错啊"

**为什么不用 LCS（最长公共子序列）？**
> 这里要求的是"A的尾巴和 B 的头重叠"（连续匹配），不是任意位置的公共子序列。后缀-前缀匹配 O(n²) 最坏，但实际重叠通常 < 20字符，性能完全够用。

---

## 五、AudioPcmRingBuffer —— 无锁环形缓冲区

```cpp
class AudioPcmRingBuffer {
    std::vector<float> buffer_;   // 底层存储
    size_t capacity_, head_, tail_, size_;
    int sample_rate_;
    double head_time_sec_;        // 当前头部对应的时间戳
    mutable std::mutex mutex_;

    size_t push(const float* data, size_t samples, double time);
    size_t peek(float* out, size_t samples) const;// 只读不消费
    size_t consume(size_t samples);                  // 前移 head
    size_t available() const;
    double head_time_sec() const;
};
```

**设计要点**：
- 默认容量 160000 = 10秒@16kHz（足够缓冲 3个窗口 + 余量）
- `push` 空间不足时 `dropOldestLocked()` 丢弃最老数据（宁丢旧数据不堵SDL回调）
- `peek` + `consume` 分离：识别线程 peek3s 给 whisper，识别完才 consume 1s
- 所有方法用 `std::lock_guard<std::mutex>` 保护（mutex，非"无锁"——因为实时音频线程不直接读此buffer，已经有 Resampler 做了一次 copy解耦）

---

## 六、AsrModelCache —— 预加载 + 引用计数 + 热切换

### 设计

```
状态机: Unloaded → Loading → Loaded (/ Failed)
                ↑
                       └── setModelPath() 触发重新加载

引用计数: tryAcquire() → ref_count_++, 返回 context
          release()     → ref_count_--
```

### 核心流程

```cpp
// 1. 主界面启动时
AsrModelCache::instance().setModelPath("/path/to/base.bin");
// → 触发 worker_thread_ 异步加载 whisper_init_from_file()

// 2. 播放视频时（AsrRealtimeStrategy::init）
whisper_context* ctx = nullptr;
if (AsrModelCache::instance().tryAcquire(ctx)) {
    worker_->initWithContext(ctx, cfg);  // 复用已加载的 context
    uses_cached_model_ = true;
} else {
    worker_->init(cfg);  // 自行加载（fallback）
}

// 3. 切换视频时（release + 再次tryAcquire）
AsrModelCache::instance().release();
// → ref_count_--，context 不释放（常驻）
```

### 热切换（模型路径变更）

```cpp
void loadInThread(const QString& path) {
    state_ = Loading;
    whisper_context* new_ctx = whisper_init_from_file(...);
    {
        QMutexLocker lock(&mtx_);
        stale_ctx_ = ctx_;    // 旧 ctx暂存
        ctx_ = new_ctx;       // 新 ctx 立即可用
    }
    // 安全释放旧 ctx（此时没人引用了）
    if (stale_ctx_) { whisper_free(stale_ctx_); stale_ctx_ = nullptr; }
    state_ = Loaded;
}
```

**为什么不直接释放旧 ctx？**
> 可能有正在识别的线程还在用旧 ctx（`ref_count_ > 0`）。暂存到 `stale_ctx_`，等所有引用释放后再 free。

---

## 七、Whisper 参数配置

```cpp
auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
params.language= "auto" ? nullptr : lang.c_str();  // 自动检测语言
params.translate= false;     // 不翻译
params.no_context = false;     // 使用上下文延续
params.single_segment = false; // 允许多段输出
params.n_threads= 4;         // 4线程推理
```

---

## 八、数据流全路径

```
SDL 音频回调(解码线程) → AudioOutput::sendAudio(AVFrame*)
    → Resampler: 任意格式 → 16kHz mono float
    → ring_.push(pcm, samples, pts)
    
识别线程(独立 std::thread):
    → ring_.peek(3s) → whisper_full() → SubtitleItem[]
    → mergeOverlap(last_text, new_text) → 去重
    → queue_->push(item) → UI 线程取出显示
    → ring_.consume(1s) →滑动窗口前移
```

---

## 九、开发中遇到的问题

### 问题 1："我我是学生学生" 类重复输出

**现象**：字幕频繁出现重复文字片段。

**原因**：3s 窗口有 2s 重叠，whisper 会把重叠部分再次识别出来。如果直接拼接，第N 次输出的前2 秒和第 N-1 次输出的后 2 秒内容重复。

**解决**：`mergeOverlap` 算法找到 A尾缀与 B 前缀的最长匹配，只拼接 B 中不重叠的新部分。加上 `last_text_` 判断，完全相同时不推送。

### 问题 2：切换视频时 whisper 首次识别延迟 800ms+

**现象**：每次换片后第一条字幕要等很久才出来。

**原因**：`whisper_init_from_file()` 需要从磁盘加载约 150MB 的模型文件（base模型），即使有系统文件缓存，首次调用仍需 600~1000ms。

**解决**：设计 `AsrModelCache` 单例，主界面启动时预加载模型常驻内存；切片时只`tryAcquire()` 获取已加载的 context 引用，跳过磁盘 IO。首字幕延迟从 "1s + 3s窗口" 降低到 "0+ 3s 窗口积累" ≈ 3s（受物理限制，需要积累 3s 音频）。

### 问题 3：快速切片时 context 被释放导致崩溃

**现象**：快速连续切换视频（<1s内切2次）时，偶发crash，堆栈在 `whisper_full()`。

**原因**：第一个视频的识别线程还在用context，第二次切片 `stop()` → `release()` 后 context 被释放。

**解决**：引入引用计数。`tryAcquire()` → `ref_count_++`，`release()` → `ref_count_--`。只有 `ref_count_ == 0` 且需要热切换时才释放旧 context。`stop()` 中先`running_ = false` + `thread_.join()` 确保识别线程退出后才release。

### 问题 4：环形缓冲区 push 时偶尔丢数据

**现象**：字幕偶尔漏掉几个字。

**原因**：环形缓冲区容量不够大时，`dropOldestLocked()` 丢弃了还未被`peek` 的数据（如 whisper 一次识别耗时超过 1s 时，新push 的数据把旧的覆盖了）。

**解决**：将默认容量从 48000（3s）扩大到 160000（10s），足够缓冲识别延迟的波动。同时在日志中记录 drop 事件以便追踪。

### 问题 5：语言自动检测不稳定，中英混合时跳来跳去

**现象**：看英文视频夹杂中文旁白时，字幕忽中忽英。

**原因**：`params.language = nullptr`（auto）时 whisper 每次 3s 窗口独立检测语言，窗口边界正好切到中文就整段按中文识别。

**解决方案（权衡）**：提供 UI 选项让用户指定语言；当设置为 "auto" 时，取前10s 检测结果做多数投票，后续窗口锁定该语言。（当前代码中简单用 "auto"，面试时可以说"这是已知限制，后续优化方向"。）

---

## 十、面试必答题

### Q1：为什么是 3s/1s 而不是 5s/2s或 2s/0.5s？
> **3s**：whisper base 模型在 3s 音频上推理 ~200ms（4线程 CPU），首字幕延迟 3s 可接受。5s 太慢。2s 短窗口上下文不够，中文识别准确率下降10%+。
> **1s步进**：2s 重叠保证不丢词；0.5s 重叠太小容易在边界断句。

### Q2：mergeOverlap 的时间复杂度？有没有更优方案？
> 最坏 O(n²)（n = min(|A|, |B|)），但实际 n < 30 字符，可忽略。KMP 可以优化到 O(n)，但 30 字符的 O(n²) 比 KMP 的常数开销还小，不值得优化。

### Q3：为什么不用流式Whisper（whisper_full_with_state + streaming）？
> whisper.cpp 原生不支持真流式（必须给完整音频段才能推理），所以用滑动窗口模拟流式。如果用 faster-whisper 等支持 VAD 切分的方案可以做到更低延迟，但会引入 Python 依赖，不适合桌面客户端。

### Q4：AsrModelCache 为什么用引用计数而不是 shared_ptr？
> `whisper_context*` 是 C 指针（whisper.cpp 是 C 库），不能直接套shared_ptr。用手动引用计数可以在 release归零时做额外逻辑（如触发重载检测），比 shared_ptr 的自定义 deleter 更灵活。

### Q5：如果用户机器很慢（whisper 推理 > 1s/窗口），会发生什么？
> 环形缓冲区会持续积累未消费数据，直到触发 `dropOldestLocked()` 丢弃旧音频。表现为"字幕延迟逐渐加大"。解决思路：检测 RTF > 1时自动降级（加大步进/跳过窗口 / 切换更小模型）。
