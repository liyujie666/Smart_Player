# 面试准备：流式AI 字幕

## 一、架构总览

重构后的字幕系统采用 **三层解耦 + 管线编排** 架构，将"音频从哪来"（IAudioSource）、"音频怎么识别"（VAD + ASR）、"识别结果怎么翻译"（Translator）彻底分离，通过 `AsrPipeline` 统一编排，`AsrManager` 管理引擎生命周期和跨文件复用。

```
                         ┌──────────────────────────────────────────┐
                         │               AsrManager                  │
                         │  (引擎缓存 + 异步初始化 + 配置管理)        │
                         │  cached_vad_ / cached_asr_ / cached_tr_   │
                         └───────────┬──────────────┬───────────────┘
                                     │ 注入引擎指针   │ 生命周期管理
                                     ▼              ▼
┌──────────────┐   feedAudio()  ┌──────────────────────────────────────┐    subtitleReady    ┌───────────────┐
│ AudioOutput  │───────────────→│            AsrPipeline                │───────────────────→│ SubtitleQueue │
│ (解码帧)      │                │                                      │                     │ → UI 显示      │
└──────────────┘                │  ┌─────────────┐  ┌────────────────┐ │  translationReady   └───────────────┘
      │                         │  │ AudioSource │  │     VAD        │ │                         ↑
      │ Push frame              │  │ (File/Live) │─→│ FsmnVad (ONNX) │ │                    ┌────┴────┐
      ▼                         │  └─────────────┘  └───────┬────────┘ │  translateQueue     │ 翻译线程 │
┌──────────────┐                │                           │ segments │  (异步,不阻塞字幕)  │          │
│ LiveAudioSrc │                │  ┌─────────────┐  ┌───────▼────────┐ │                     └────┬────┘
│ (Push 模式)   │                │  │ RingBuffer  │  │  ASR Engine    │ │                          │
│ Resampler    │                │  │ 10s @16kHz  │  │ Whisper/SenseV │ │                     ┌────▼────┐
│ →16kHz mono  │                │  └─────────────┘  │ CloudASR       │ │                     │Translator│
└──────────────┘                │                   └───────┬────────┘ │                     │ GPT/NLLB │
                                │                           │ text     │                     │ Marian/TC│
                                │                   ┌───────▼────────┐ │                     └─────────┘
                                │                   │  SubtitleQueue  │ │
                                │                   │  (线程安全)     │ │
                                │                   └────────────────┘ │
                                └──────────────────────────────────────┘
```

**核心思想**：

1. **IAudioSource 抽象**：音频获取与识别逻辑解耦。Pull 模式（文件）自主 Demux→Decode→Resample；Push 模式（RTSP/RTMP）接收外部解码帧写入 RingBuffer。
2. **VAD 前置**：用 FSMN-VAD（ONNX）检测语音活动区间，只在有语音的段做 ASR，避免对静音段浪费推理资源。
3. **多引擎可插拔**：ASR（Whisper/SenseVoice/Cloud）、VAD（FSMN/Silero）、翻译（GPT/NLLB/MarianMT/Tencent）均通过接口 + 工厂模式创建，运行时可切换。
4. **异步翻译**：翻译在独立线程执行，不阻塞字幕原文显示。
5. **引擎跨文件复用**：AsrManager 缓存引擎实例，切换视频时不重新加载模型。

---

## 二、接口抽象设计

### 四大核心接口

```cpp
// ASR 引擎：输入 PCM，输出带时间戳的字幕段
class IAsrEngine {
    virtual bool init(const AsrEngineConfig& cfg) = 0;
    virtual bool recognize(const std::vector<float>& pcm,
                           std::vector<SubtitleItem>& out,
                           double base_sec = 0.0) = 0;
    virtual void reset() = 0;
    virtual bool isReady() const = 0;
    virtual std::string name() const = 0;
};

// VAD 引擎：流式输入音频块，返回完整语音段
class IVadEngine {
    virtual std::vector<VadSegment> process(const std::vector<float>& pcm,
                                            double base_sec = 0.0) = 0;
    virtual std::vector<VadSegment> flush() = 0;  // 强制刷新残余
    virtual void reset() = 0;
};

// 音频源：产出归一化 PCM (16kHz/mono/float32)
class IAudioSource {
    virtual AudioSourceMode mode() const = 0;     // Pull or Push
    virtual int pull(float* out, int max, double& time) = 0;  // Pull 模式
    virtual void pushFrame(AVFrame* frame) = 0;               // Push 模式
    virtual void peek(float* out, size_t n) const = 0;        // Push 模式
    virtual void consume(size_t n) = 0;                        // Push 模式
};

// 翻译引擎
class ITranslator {
    virtual TranslateResult translate(const std::string& text) = 0;
    virtual std::vector<TranslateResult> translateBatch(
        const std::vector<std::string>& texts) = 0;
};
```

### 工厂模式

```cpp
std::unique_ptr<IAsrEngine>   createAsrEngine(AsrEngineType type);      // Whisper / SenseVoice / CloudASR
std::unique_ptr<IVadEngine>   createVadEngine(VadEngineType type);      // FSMN / Silero
std::unique_ptr<ITranslator>  createTranslator(TranslatorType type);    // GPT / NLLB / MarianMT / TencentCloud
```

### 引擎实现一览

| 接口 | 实现 | 特点 |
|------|------|------|
| `IAsrEngine` | `WhisperEngine` | whisper.cpp 本地推理，支持 AsrModelCache 缓存 context |
| | `SenseVoiceEngine` | FunASR SenseVoice ONNX 模型，FBank→LFR→CTC 解码 |
| | `CloudAsrEngine` | 腾讯云一句话识别 API，网络请求 |
| `IVadEngine` | `FsmnVad` | FunASR FSMN-VAD ONNX，流式状态机 + 概率平滑 |
| `ITranslator` | `GptTranslator` | OpenAI 兼容 API（支持 ollama 等本地部署） |
| | `NllbTranslator` | Meta NLLB 本地推理，200+ 语言 |
| | `MarianMtTranslator` | MarianMT 本地推理，特定语言对性能优秀 |
| | `TencentTranslator` | 腾讯云 TMT 批量翻译 API |
| `IAudioSource` | `FileAudioSource` | Pull 模式，自带 Demuxer+Decoder+Resampler |
| | `LiveAudioSource` | Push 模式，接收外部帧写入 RingBuffer |

---

## 三、AsrPipeline —— 管线编排核心

`AsrPipeline` 统一编排 VAD → ASR → Translation 三级流水线，支持实时和离线两种模式，由 `AsrManager` 创建并注入引擎指针。

### 3.1 两种处理线程

```cpp
void AsrPipeline::start() {
    if (source_->mode() == AudioSourceMode::Pull) {
        vad_asr_thread_ = std::thread(&AsrPipeline::offlineLoop, this);   // 离线
    } else {
        vad_asr_thread_ = std::thread(&AsrPipeline::vadAsrLoop, this);    // 实时
    }
    if (config_.enable_translation) {
        translate_thread_ = std::thread(&AsrPipeline::translateLoop, this);
    }
}
```

### 3.2 实时模式 vadAsrLoop —— 3s 窗口 / 1s 步进

```cpp
void AsrPipeline::vadAsrLoop() {
    const int SR = 16000;
    const size_t win = 3 * SR;    // 3s 窗口
    const size_t step = 1 * SR;   // 1s 步进
    std::vector<float> buf(win);

    while (running_) {
        // 1. 等待足够数据（优先用 source_，回退到内部 ring_）
        size_t avail = use_source ? source_->available() : ring_.available();
        if (avail < win) { sleep(2ms); continue; }

        // 2. peek 3s（不消费）
        source_->peek(buf.data(), win);
        double start_time = source_->headTimeSec();

        // 3. VAD → ASR（有语音段才识别）
        if (config_.enable_vad)
            processVadAudio(buf, start_time);    // VAD 分段 → ASR
        else
            processDirectAsr(buf, start_time);   // 无 VAD 直接 ASR

        // 4. consume 1s → 2s 重叠
        source_->consume(step);
    }
    // 刷新 VAD 残余
    processVadSegments(vad_->flush());
}
```

**与旧架构的区别**：旧架构直接对 3s 窗口做 Whisper 识别 + mergeOverlap 去重；新架构在 ASR 前加了 VAD，只对检测到的语音段做识别，大幅减少静音段的无效推理。

### 3.3 离线模式 offlineLoop —— Pull + 自适应节流

```cpp
void AsrPipeline::offlineLoop() {
    // 降低线程优先级，避免抢占播放
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    const int NORMAL_CHUNK = (asr_type == Whisper) ? 30 : 10;  // 秒
    const int SEEK_CHUNK = 3;  // seek 后用更小 chunk 快速响应

    double ema_rtf = 0.5;  // RTF 指数移动平均

    while (running_ && !source_->isEof()) {
        int chunk = seek_flag_ ? SEEK_CHUNK : NORMAL_CHUNK;
        double media_time;
        int n = source_->pull(pcm, chunk * SR, media_time);

        // === 三阶段自适应节流 ===
        // 阶段3：超前等待 — 识别进度超前 lookahead 太多时暂停
        if (media_time - playback_pos > lookahead_sec)
            sleep(precise_wait_ms);

        // 识别处理
        auto t0 = steady_clock::now();
        processVadAudio(chunk, media_time);  // 或 processDirectAsr
        auto t1 = steady_clock::now();

        // 更新 RTF
        double rtf = elapsed_ms / (chunk_sec * 1000);
        ema_rtf = 0.3 * rtf + 0.7 * ema_rtf;

        // 阶段1/2 自适应 yield
        double ahead = media_time - playback_pos;
        if (ahead <= 0)
            yield_ms = 2;                      // 追赶阶段：最小 yield
        else if (ahead < lookahead_sec)
            yield_ms = (spare_ms > 0) ? spare_ms * 0.7 : 2;  // 跟随阶段
    }
}
```

**三阶段节流模型**：

| 阶段 | 条件 | 策略 | 目的 |
|------|------|------|------|
| 追赶 | `ahead ≤ 0` | yield=2ms | 识别落后播放，尽快追赶 |
| 跟随 | `0 < ahead < lookahead` | yield=spare×0.7 | 匀速推进，保持 lookahead 稳定 |
| 超前 | `ahead ≥ lookahead` | 精确等待 | 暂停识别，避免浪费 CPU |

**RTF（Real-Time Factor）**：`RTF = 处理耗时 / chunk 音频时长`。RTF < 1 说明识别比实时快；RTF ≥ 1 说明 CPU 压力大，减小 yield。用 EMA 平滑避免单次波动导致抖动。

---

## 四、VAD —— FSMN-VAD 神经网络语音活动检测

### 4.1 为什么需要 VAD？

旧架构直接对 3s 滑窗做 ASR，静音段也被送入 Whisper 推理，浪费大量 CPU。新架构前置 FSMN-VAD，只输出有语音的段（start_sec ~ end_sec），ASR 只识别这些段。

### 4.2 FSMN-VAD 架构

```
PCM (16kHz/mono/float32)
    ↓
FBank 特征提取（80 维 Mel 频谱，25ms 窗口 / 10ms 步进）
    ↓
LFR 拼接（m=5, n=1 → 400 维）+ CMVN 归一化
    ↓
FSMN ONNX 推理（输入: speech + 4 个 cache 隐状态）
    ↓
输出: 每帧 speech 概率 (T 帧 × 2 类，取 1 - silence_posterior)
    ↓
概率滑动平均平滑（window=3）
    ↓
迟滞阈值状态机（Silence → Speech → Trailing → Silence）
    ↓
输出: VadSegment { start_sec, end_sec }
```

### 4.3 流式状态机 —— 迟滞阈值 + 最小静音断句

```cpp
void FsmnVad::updateState(float speech_prob, double frame_time) {
    // 迟滞阈值：进入用高阈值，退出用低阈值（防止边界振荡）
    bool is_speech = (state_ == Silence)
        ? speech_prob >= threshold       // 0.5 进入
        : speech_prob >= threshold_exit; // 0.7 退出

    switch (state_) {
    case Silence:
        if (is_speech) → Speech (记录 start)
        break;
    case Speech:
        if (!is_speech) → Trailing (开始计静音帧)
        else 更新 end
        break;
    case Trailing:
        if (is_speech) → Speech (恢复)
        else if (静音帧数 >= min_silence_ms/10ms)
            → 输出段 {start, end} → Silence
        break;
    }

    // 单段超过 max_speech_ms(8s) 强制断句
    if (duration >= max_speech_ms) { 输出段; 重新开始; }
}
```

**关键参数**（VadConfig）：
- `threshold = 0.5`：进入语音阈值（高，避免误触发）
- `threshold_exit = 0.7`：退出语音阈值
- `min_silence_ms = 150`：最短静音间隔即断句（敏感）
- `min_speech_ms = 250`：过滤过短的噪声段
- `max_speech_ms = 8000`：单段最长 8 秒，超过强制断句
- `smoothing_window = 3`：概率滑动平均窗口

### 4.4 流式连续性处理

VAD 在连续输入块之间需要保持状态一致性：

```cpp
// AsrPipeline::processVadAudio
// 1. 时间戳连续性检查
double delta = base_sec - buffered_end;
if (delta > tolerance) {
    vad_->reset();           // 音频不连续 → 重置 VAD
    vad_pcm_buffer_.clear();
}

// 2. 保留跨块 PCM（语音段可能跨越多个输入块）
vad_pcm_buffer_.insert(vad_pcm_buffer_.end(), new_audio.begin(), new_audio.end());

// 3. 限制缓冲大小（30s = 480000 samples）
if (vad_pcm_buffer_.size() > 30 * SR) {
    // 丢弃最旧的部分，同步调整 base_sec
}

// 4. VAD 输出段 → 从 vad_pcm_buffer_ 切片 → 送 ASR
for (auto& seg : segments) {
    auto speech_pcm = vad_pcm_buffer_[start_sample..end_sample];
    asr_->recognize(speech_pcm, results, seg.start_sec);
}
```

### 4.5 FSMN 隐状态缓存

FSMN 是有状态的模型，通过 cache 输入/输出维持跨帧上下文：

```cpp
// 输入: speech (1,T,400) + cache0~3 (隐藏状态)
// 输出: logits (1,T,2) + cache0~3' (更新后的隐藏状态)

// 每次推理后，将输出的 cache 回写
for (size_t i = 0; i < caches_.size(); ++i) {
    std::copy(output_cache_data, output_cache_data + count, caches_[i].begin());
}
```

`reset()` 时清零所有 cache，实现 VAD 状态重置（seek / 音频不连续时）。

---

## 五、ASR 引擎

### 5.1 WhisperEngine

```cpp
bool WhisperEngine::recognize(const std::vector<float>& pcm, ...) {
    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = (cfg_.language == "auto") ? nullptr : cfg_.language.c_str();
    params.translate = false;
    params.no_context = false;      // 使用上下文延续
    params.single_segment = false;  // 允许多段输出
    params.n_threads = 4;

    whisper_full(ctx_, params, pcm.data(), pcm.size());

    // 提取每个 segment 的文本和时间戳
    for (int i = 0; i < n_segments; ++i) {
        item.text = AsrUtil::normalizeText(whisper_full_get_segment_text(...));
        item.start_sec = base_sec + whisperTsToSec(t0);
        item.end_sec = base_sec + whisperTsToSec(t1);
    }
}
```

支持两种初始化方式：
- `init()`：自行从磁盘加载模型（`owns_context_ = true`）
- `initWithContext()`：使用 AsrModelCache 预加载的 context（`owns_context_ = false`，不负责释放）

### 5.2 SenseVoiceEngine（ONNX 本地推理）

```
PCM → FBank(80维) → LFR(m=7,n=6 → 560维) → CMVN → ONNX 推理 → CTC Greedy 解码 → 后处理
```

**与 Whisper 的区别**：
- Whisper 输入是原始 PCM（whisper.cpp 内部做特征提取）
- SenseVoice 需要手动做 FBank → LFR → CMVN 前处理
- SenseVoice 输出是 CTC token 序列，需要手动解码 + 去标签
- SenseVoice ONNX 线程数限制为 2（避免抢占视频解码）
- 建议 chunk ≤ 10 秒（Whisper 设计处理 30 秒）

**后处理**：去除 SenseVoice 特殊标签 `<|zh|>` `<|HAPPY|>` `<|Speech|>` 等，替换 SentencePiece 词边界符 `\xE2\x96\x81` 为空格。

### 5.3 CloudAsrEngine

将 PCM 转 16bit → base64 编码 → 调用腾讯云一句话识别 API → 解析 JSON 响应。无本地模型依赖，但需要网络且延迟较高。

---

## 六、翻译子系统 —— 异步管线

### 6.1 异步翻译线程

翻译在独立线程执行，不阻塞字幕原文显示：

```cpp
void AsrPipeline::translateLoop() {
    while (translate_running_) {
        SubtitleItem item;
        {
            std::unique_lock<std::mutex> lock(translate_mtx_);
            translate_cv_.wait(lock, [] { return !queue.empty() || !running; });
            item = translate_queue_.front();
            translate_queue_.pop();
        }

        auto result = translator_->translate(item.text);
        if (result.success) {
            item.translated_text = result.translated_text;
            queue_->push(item);              // 回填译文到字幕队列
            emit translationReady(item);     // 通知 UI 更新
        }
    }
}
```

### 6.2 翻译引擎选择策略

| 引擎 | 场景 | 优缺点 |
|------|------|--------|
| GPT | 通用、高质量翻译 | 需 API Key，有延迟，但质量最好且支持任意语言对 |
| NLLB | 离线、多语言 | 200+ 语言，本地推理，但模型较大 |
| MarianMT | 离线、特定语言对 | 特定语言对（如 en→zh）性能优秀，模型较小 |
| TencentCloud | 国内场景 | 腾讯云 TMT 批量翻译，国内延迟低 |

### 6.3 动态开关翻译

```cpp
void AsrManager::applyTranslationToggle(bool enabled) {
    translation_enabled_ = enabled;

    if (!pipeline_) return;  // 无运行中的管线，仅更新标志

    if (enabled && !cached_translator_) {
        // 懒加载翻译引擎
        cached_translator_ = createTranslator(cfg.translator_type);
        cached_translator_->init(cfg.translate_config);
        pipeline_->setTranslatorEngine(cached_translator_.get());
    }

    pipeline_->enableTranslation(enabled);  // 动态启停翻译线程
}
```

---

## 七、AsrManager —— 引擎生命周期管理

### 7.1 引擎跨文件复用

```cpp
bool AsrManager::initInternal(...) {
    // 1. 创建音频源
    source = (type == RTSP/RTMP) ? LiveAudioSource : FileAudioSource;

    // 2. 检查引擎是否需要重建（仅在类型/模型路径变化时）
    bool need_rebuild = !cached_asr_ ||
        cached_asr_type_ != cfg.asr_type ||
        cached_asr_model_path_ != cfg.asr_config.model_path;

    if (need_rebuild) {
        cached_asr_ = createAsrEngine(cfg.asr_type);
        cached_asr_->init(cfg.asr_config);    // 加载模型（耗时）
    } else {
        cached_asr_->reset();                  // 复用，仅重置状态
    }

    // 3. 创建 Pipeline 并注入引擎指针（Pipeline 不拥有引擎）
    pipeline_ = std::make_unique<AsrPipeline>();
    pipeline_->setSource(std::move(source));
    pipeline_->setVadEngine(cached_vad_.get());      // 注入，不析构
    pipeline_->setAsrEngine(cached_asr_.get());
    pipeline_->setTranslatorEngine(cached_translator_.get());
}
```

**关键设计**：Pipeline 持有引擎的裸指针（不拥有），AsrManager 持有 `unique_ptr`（拥有）。切换视频时 Pipeline 重建，引擎实例复用，跳过模型加载。

### 7.2 异步初始化

```cpp
void AsrManager::initAsync(...) {
    init_thread_ = std::thread([this, ...] {
        std::lock_guard<std::mutex> lock(init_mtx_);
        if (pipeline_) { pipeline_->stop(); pipeline_.reset(); }
        queue_.clear();
        bool ok = initInternal(...);    // 模型加载在工作线程
        if (ok) pipeline_->start();
    });
}
```

ONNX 模型加载可能耗时数秒，放在工作线程避免阻塞 UI。

### 7.3 非阻塞 stop

```cpp
void AsrManager::stop() {
    init_cancelling_ = true;
    if (init_thread_.joinable()) init_thread_.detach();

    std::lock_guard<std::mutex> lock(init_mtx_);
    if (pipeline_) {
        // 异步停止 + 析构，不阻塞主线程
        std::thread([pipeline = std::move(pipeline_)]() mutable {
            pipeline->stop();
            pipeline.reset();
        }).detach();
    }
    queue_.clear();
}
```

---

## 八、AsrModelCache —— Whisper 模型预加载

### 8.1 设计

```
状态机: Unloaded → Loading → Loaded (/ Failed)
                ↑                    ↓
                └── setModelPath()   tryAcquire() → ref_count_++
                                     release()     → ref_count_--
                                     (ref_count_==0 时触发重载)
```

### 8.2 核心流程

```cpp
// 1. 主界面启动时预加载
AsrModelCache::instance().setModelPath("/path/to/base.bin");
// → 异步 whisper_init_from_file_with_params()

// 2. 播放视频时（WhisperEngine::init）
whisper_context* ctx = nullptr;
if (AsrModelCache::instance().tryAcquire(ctx)) {
    whisperEngine->initWithContext(ctx, cfg);  // 复用已加载的 context
}

// 3. 切换视频时
AsrModelCache::instance().release();
// → ref_count_--，context 不释放（常驻内存）
```

### 8.3 热切换（模型路径变更）

```cpp
void loadInThread(const QString& path) {
    state_ = Loading;
    whisper_context* new_ctx = whisper_init_from_file_with_params(...);
    {
        QMutexLocker lock(&mtx_);
        stale_ctx_ = ctx_;     // 旧 ctx 暂存
        ctx_ = new_ctx;        // 新 ctx 立即可用
    }
    // 安全释放旧 ctx（此时新 ctx 已就绪）
    if (stale_ctx_) { whisper_free(stale_ctx_); stale_ctx_ = nullptr; }
    state_ = Loaded;
}
```

**为什么不直接释放旧 ctx？**
> 可能有正在识别的线程还在用旧 ctx（`ref_count_ > 0`）。暂存到 `stale_ctx_`，等新 ctx 加载完成且切换后，再安全释放旧 ctx。

---

## 九、AudioPcmRingBuffer —— 环形缓冲区

```cpp
class AudioPcmRingBuffer {
    std::vector<float> buffer_;   // 底层存储
    size_t capacity_, head_, tail_, size_;
    int sample_rate_;
    double head_time_sec_;        // 当前头部对应的时间戳
    mutable std::mutex mutex_;

    size_t push(const float* data, size_t samples, double time);  // 写入
    size_t peek(float* out, size_t samples) const;                // 只读不消费
    size_t consume(size_t samples);                                // 前移 head
    double head_time_sec() const;
};
```

**设计要点**：
- 默认容量 160000 = 10 秒 @ 16kHz（足够缓冲 3 个 3s 窗口 + 余量）
- `push` 空间不足时 `dropOldestLocked()` 丢弃最老数据（宁丢旧数据不堵音频回调）
- `peek` + `consume` 分离：识别线程 peek 3s 给 ASR，识别完才 consume 1s
- `head_time_sec_` 跟踪头部时间戳，`consume` 时按采样数推进
- 所有方法用 `std::lock_guard<std::mutex>` 保护

---

## 十、数据流全路径

### 10.1 实时模式（RTSP/RTMP）

```
播放器解码线程:
  AudioOutput::sendAudio(AVFrame*)
    → LiveAudioSource::pushFrame(frame)
        → Resampler: 任意格式 → 16kHz mono float32
        → ring_.push(pcm, samples, pts)

AsrPipeline 实时线程 (vadAsrLoop):
  → ring_.peek(3s)
  → VAD: processVadAudio(buf, start_time)
      → FsmnVad::process() → VadSegment[]
      → 从 vad_pcm_buffer_ 切片 → ASR recognize
  → SubtitleItem → queue_->push() → emit subtitleReady
  → (可选) translate_queue_ → 翻译线程
  → ring_.consume(1s) → 滑窗前移
```

### 10.2 离线模式（本地文件）

```
AsrPipeline 离线线程 (offlineLoop):
  → FileAudioSource::pull(30s chunk)  ← 自主 Demux→Decode→Resample
  → 超前等待检查（lookahead）
  → VAD: processVadAudio(chunk, media_time)
      → FsmnVad::process() → VadSegment[]
      → 从 vad_pcm_buffer_ 切片 → ASR recognize
  → SubtitleItem → queue_->push() → emit subtitleReady
  → (可选) translate_queue_ → 翻译线程
  → 自适应节流 yield（追赶/跟随/超前三阶段）
  → 下一个 chunk
```

### 10.3 翻译线程（独立）

```
translateLoop:
  → wait(translate_queue_)
  → translator_->translate(item.text)
  → item.translated_text = result
  → queue_->push(item)  // 回填译文
  → emit translationReady(item)
```

---

## 十一、Seek 处理

```cpp
void AsrPipeline::seekTo(double pos_sec) {
    // 1. Seek 音频源
    source_->seekTo(pos_sec);   // Pull: seek demuxer; Push: clear ring

    // 2. 清空所有内部缓冲和引擎状态
    ring_.clear();
    vad_pcm_buffer_.clear();
    vad_->reset();    // 清零 FSMN 隐状态
    asr_->reset();    // 清零 ASR 上下文
    translate_queue_ = {};

    // 3. 清空字幕队列（旧字幕时间戳已无效）
    queue_->clear();

    // 4. 设置 seek 标志 → offlineLoop 用 3s 小 chunk 快速响应
    seek_flag_ = true;
}
```

**seek_flag_ 的作用**：seek 后第一个 chunk 用 3 秒（而非 30 秒），让 ASR 尽快产出字幕，同时跳过超前等待和节流。

---

## 十二、FBank 特征提取

FBank（Mel Filterbank）是 SenseVoice 和 FSMN-VAD 的共享前端：

```
PCM [-1,1] float32
  → × 32768 (缩放到 int16 幅度范围，匹配 CMVN 训练条件)
  → 去直流分量 + 预加重 (0.97)
  → 汉明窗 (25ms = 400 samples)
  → FFT (512 点) → 功率谱
  → Mel 滤波器组 (80 维) → log
  → 80 维 FBank 特征
```

**为什么 PCM 要乘 32768？**
> FunASR/Kaldi 的 FBank 训练时假设输入是 int16 量化的浮点表示。当前重采样器输出 `[-1, 1]` 归一化浮点，需要放大到 int16 幅度范围以匹配 CMVN 训练条件，否则特征数值分布与训练时不一致，导致识别准确率下降。

---

## 十三、开发中遇到的问题

### 问题 1：VAD 与 ASR 的时间戳对齐

**现象**：VAD 输出的语音段时间戳与实际 PCM 位置不匹配，导致 ASR 识别到的文本时间戳偏移。

**原因**：VAD 内部维护帧计数 `total_frames_processed_`，但跨块输入时 PCM 拼接可能导致帧计数与实际 PCM 位置不一致。

**解决**：在 AsrPipeline 中维护独立的 `vad_pcm_buffer_` 和 `vad_pcm_base_sec_`，VAD 输出段后用 `seg.start_sec - vad_pcm_base_sec_` 换算到 buffer 内的采样位置，确保切片准确。同时检测音频不连续（`delta > tolerance`）时重置 VAD。

### 问题 2：FSMN-VAD 量化模型输出不是 logits 而是后验概率

**现象**：最初对 FSMN-VAD 输出做 softmax 后取 speech 类概率，导致 VAD 几乎不触发。

**原因**：FunASR 导出的量化 ONNX 模型输出已经是后验概率（PDF），不需要再 softmax。248 类中 class_0 是静音，需要取反 `1 - silence_posterior`。

**解决**：直接使用模型输出作为后验概率，`probs[t] = 1.0 - row[0]`（class_0 = silence posterior）。

### 问题 3：离线识别全速运行导致播放卡顿

**现象**：开启字幕后，一开场就全速识别整个文件，CPU 占满导致视频播放卡顿。

**原因**：离线模式没有节流，识别线程以最大速度 pull + 推理，与播放线程抢 CPU。

**解决**：设计三阶段自适应节流模型（追赶/跟随/超前），基于 RTF（指数移动平均）动态调整 yield 时间。同时降低识别线程优先级（`THREAD_PRIORITY_BELOW_NORMAL`）。超前播放位置超过 lookahead（20s）时精确等待。

### 问题 4：快速切换视频时引擎被释放导致崩溃

**现象**：快速连续切换视频时，偶发 crash 在 `whisper_full()` 或 ONNX 推理中。

**原因**：旧 Pipeline 的识别线程还在用引擎，新 initInternal 重建引擎导致 use-after-free。

**解决**：
1. `stop()` 中先 `running_ = false` + `thread_.join()` 确认识别线程退出
2. `AsrManager::stop()` 中 pipeline 的 stop + 析构放到独立线程异步执行（避免阻塞主线程），但 `initInternal` 前会 `join` 旧 pipeline
3. 引擎实例由 AsrManager 持有（`unique_ptr`），Pipeline 只持有裸指针引用，Pipeline 析构不影响引擎

### 问题 5：SenseVoice LFR 维度不匹配

**现象**：SenseVoice 推理输出全 blank token。

**原因**：模型输入维度是 560（80×7 LFR），但最初用 80 维直接输入，特征维度不匹配导致推理输出无意义。

**解决**：从模型 shape 自动推断 `model_feat_dim`，计算 `lfr_m = model_feat_dim / 80`，正确做 LFR(m=7, n=6) 拼接。同时校验 CMVN 维度与 feat_dim 一致。

### 问题 6：Seek 后字幕长时间不出现

**现象**：Seek 后第一条字幕要等很久才出来。

**原因**：seek 后仍用 30 秒 chunk，加上超前等待节流，导致字幕延迟很大。

**解决**：引入 `seek_flag_`，seek 后第一个 chunk 用 3 秒（`SEEK_CHUNK_SEC = 3`），同时跳过超前等待和节流（`skip_throttle = true`），让 ASR 尽快产出首条字幕。

### 问题 7：mergeOverlap —— 滑窗重叠去重

**现象**：3s 窗口有 2s 重叠，Whisper 会把重叠部分再次识别出来，导致重复文字。

**解决**：`mergeOverlap` 找 A 尾缀与 B 前缀的最长匹配（后缀-前缀匹配），只拼接 B 中不重叠的新部分。加上 `last_text_` 判断，完全相同时不推送。

```cpp
inline std::string mergeOverlap(const std::string& a, const std::string& b) {
    std::string A = trim(a), B = trim(b);
    if (A.empty()) return B; if (B.empty()) return A;
    size_t max = std::min(A.size(), B.size());
    for (size_t l = max; l > 0; l--) {
        if (A.substr(A.size()-l) == B.substr(0,l)) return A + B.substr(l);
    }
    return B;
}
```

---

## 十四、面试必答题

### Q1：为什么引入 VAD 而不是直接滑窗 ASR？

> 旧架构直接对 3s 滑窗做 Whisper，静音段也被推理，浪费 CPU。引入 FSMN-VAD 后，只对有语音的段做 ASR，静音段直接跳过。VAD 本身很轻量（单线程 ONNX，64 帧/次推理），远比 Whisper/SenseVoice 快，净收益显著。同时 VAD 提供了精确的语音边界，字幕时间戳更准确。

### Q2：VAD 和 ASR 的时间戳如何对齐？

> AsrPipeline 维护 `vad_pcm_buffer_` 和 `vad_pcm_base_sec_`。VAD 输出 `VadSegment{start_sec, end_sec}` 后，用 `seg.start_sec - vad_pcm_base_sec_` 换算到 buffer 内的采样偏移，从 buffer 切片出对应 PCM 送 ASR。音频不连续时（`delta > 0.02s`）重置 VAD 状态和缓冲。

### Q3：为什么离线模式需要自适应节流？三阶段怎么设计？

> 不节流的话识别线程全速跑，与播放线程抢 CPU 导致卡顿。三阶段：追赶阶段（识别落后播放）最小 yield 尽快追赶；跟随阶段（已追上）按播放速率匀速推进，yield = chunk 时长 - 处理耗时；超前阶段（超前 lookahead 太多）精确等待。RTF 用 EMA 平滑避免单次波动抖动。

### Q4：引擎跨文件复用怎么实现的？

> AsrManager 持有 `unique_ptr<IAsrEngine> cached_asr_` 等，Pipeline 只持有裸指针。切换视频时 Pipeline 重建（stop + reset），但引擎实例不销毁，仅调用 `reset()` 清零状态。只有引擎类型或模型路径变化时才重建引擎。这样切换视频不用重新加载模型（Whisper base 150MB 加载需 600-1000ms）。

### Q5：为什么翻译要放在独立线程？

> 翻译涉及网络请求（GPT/Tencent）或本地推理（NLLB/MarianMT），延迟不确定。如果与 ASR 在同一线程，翻译慢会阻塞字幕原文显示。独立线程 + 条件变量队列：ASR 产出字幕后立即显示原文，翻译完成后通过 `translationReady` 信号回填译文，UI 分别更新。

### Q6：AsrModelCache 为什么用引用计数？

> `whisper_context*` 是 C 指针（whisper.cpp 是 C 库），不能直接套 shared_ptr。手动引用计数可以在 release 归零时触发重载检测，比 shared_ptr 的自定义 deleter 更灵活。`tryAcquire()` → `ref_count_++`，`release()` → `ref_count_--`。热切换时旧 ctx 暂存到 `stale_ctx_`，等新 ctx 就绪后再释放。

### Q7：FSMN-VAD 的状态机为什么用迟滞阈值？

> 如果进入和退出用同一个阈值，语音概率在阈值附近波动会导致状态频繁切换（Silence↔Speech），产生大量碎片段。迟滞阈值：进入用高阈值（0.5），退出用更低阈值（0.7 — 注意取反后退出要求更高概率），留出迟滞区间。加上 `min_silence_ms=150` 要求持续静音才断句，进一步防抖。

### Q8：Push 和 Pull 两种音频源模式有什么区别？

> **Pull 模式**（FileAudioSource）：Pipeline 主动调用 `pull()` 拉取 PCM，音频源内部自带 Demuxer→Decoder→Resampler 全链路，适合离线文件。**Push 模式**（LiveAudioSource）：外部播放器解码后通过 `pushFrame()` 送入帧，内部 Resample 后写入 RingBuffer，Pipeline 用 peek/consume 读取，适合 RTSP/RTMP 实时流。两种模式通过 `IAudioSource` 接口统一，Pipeline 根据 `mode()` 选择处理线程。

### Q9：如果用户机器很慢（ASR 推理 RTF > 1），会发生什么？

> 离线模式：RTF > 1 意味着识别比实时慢，追赶阶段 yield 最小化（2ms），但识别仍会越来越落后播放。表现为字幕延迟逐渐加大。当落后太多时可以考虑加大 chunk 大小（减少推理次数的常数开销）或切换更小的模型。实时模式：环形缓冲区会持续积累未消费数据直到触发 `dropOldestLocked()` 丢弃旧音频，表现为字幕延迟加大且可能漏词。
