# 面试准备：AI视频总结面板

## 一、架构总览

```
┌───────────────────────────────────────────────────────────────────┐
│  SummaryPanel (View)            TranscriptPanel (View)            │
│  - TL;DR / 要点 / 章节列表      - 逐字高亮文稿 / 搜索            │
│  - 实体标签云(可点击跳转)        - 章节折叠 / 自动滚动            │
├───────────────────────────────────────────────────────────────────┤
│  SummaryViewModel (ViewModel)                │
│  - start() / stop() / 缓存命中判断                │
├───────────────────────────────────────────────────────────────────┤
│  VideoSummaryManager (Model/Service)← Worker QThread             │
│  ┌───────────────────── 六阶段流水线 ─────────────────────┐      │
│  │ 1. extractFrames (ffmpeg 抽帧 2.5s/帧)                 │      │
│  │ 2. runWhisperASR (30s分段离线识别)│      │
│  │ 3. classifyVideoScenes (VLM 15s/帧 打标签)             │      │
│  │ 4. runSemanticSegmentation (TextTiling + 视觉融合)      │      │
│  │ 5. analyzeSegments (逐段VLM 多模态分析)               │      │
│  │ 6. generateFullReport (LLM 结构化报告)                  │      │
│  └─────────────────────────────────────────────────────────┘      │
│+ SummaryNetworkClient (qwen-vl-plus / qwen-plus API)            │
│  + SemanticSegmenter (TextTiling 自研算法)                         │
│  + 缓存层(SHA256(路径+大小+mtime) → JSON 落盘)                   │
└───────────────────────────────────────────────────────────────────┘
```

---

## 二、六阶段流水线详解

### 阶段 1：抽帧 `extractFrames()`

```
间隔: 2.5 秒/帧
方式: QProcess 调ffmpeg → image2pipe → JPEG bytes
存储: QMap<qint64, QByteArray> m_extractedFrames (时间戳→JPEG)
```

1h 视频 ≈ 1440 帧，每帧 JPEG 约 50KB → 总内存 ~70MB。

### 阶段 2：离线 ASR `runWhisperASR()`

```
1. ffmpeg 提取音频 → 16kHz/mono/PCM WAV 临时文件
2. Demuxer + Decoder + Resampler 解码PCM
3. 每累积 30s PCM → AsrWorker::recognize()
4. 结果追加到 m_asrResults: QList<SubtitleItem>
```

### 阶段 3：场景分类 `classifyVideoScenes()`

```
采样间隔: 15 秒
调用: VLM (qwen-vl-plus) 对每帧图片打标签
并发: QSemaphore{5} 令牌桶，tryAcquire 失败 → 100ms 重试
输出: QList<QPair<qint64, QString>> m_sceneTags

去抖三步:
  ① 空标签填补（失败帧用邻近非空标签）
  ② 中值滤波 K=2（前后各2帧众数投票）
  ③ 单帧噪声吸收（A-B-A → A-A-A）
```

**场景分类 Prompt**：
```
请用1-3 个英文单词描述这个画面的场景类型，
如：outdoor_talking / code_demo / slide_presentation /
product_shot / screen_recording / indoor_meeting ...
只返回场景标签，不要其他任何内容。
```

### 阶段 4：语义分段 `runSemanticSegmentation()`

```
┌──────────────────────────────────────────────────────────┐
│  SemanticSegmenter::computeSegments()                     │
│                                                           │
│  音频侧(detectAudioBoundaries):                         │
│  1. 相邻 ASR 句子 → TF向量(512维hash) + L2归一化         │
│  2. 余弦相似度 → 相似度序列                │
│  3. 滑窗平滑 W=5│
│  4. depth = (前W均+ 后W均)/2 - 当前                     │
│  5. 阈值 = mean + 0.3*std + 0.005│
│  6. 局部极大值 = 边界                │
│                                                           │
│  视觉侧 (detectVideoBoundaries):                         │
│  标签序列前后不同= 切换点│
│                                                           │
│  融合 (fuseAndSegment):                                   │
│  音频边界×0.6 + 视频边界×0.4                              │
│→ 2s 时间窗聚合 → 分数阈值过滤(mean*0.3)│
│→ adaptiveSegmentLength(3s~120s 整形)                    │
└──────────────────────────────────────────────────────────┘
```

### 阶段 5：逐段 VLM 分析 `analyzeSegments()`

每个segment：
1. 从 `m_extractedFrames` 中找距离最近的 2 帧 JPEG
2. 拼该段的 `speechText`（ASR 聚合文本）
3. 发给 VLM (qwen-vl-plus)

**帧分析 Prompt**：
```
【时间范围】{timeRange}
【该时段的语音内容】"{speechText}"
请同时结合画面和语音，描述这个时间段发生了什么。
注意人物的话语/旁白和画面动作的配合关系。
如果画面中有文字，请转录出来。
请用2-3句话描述。
```

### 阶段 6：生成报告 `generateFullReport()`

将所有段落描述 + 完整 ASR 塞给 LLM (qwen-plus，纯文本模型)：

**System Prompt 要求输出**：
```json
{
  "tldr": "一句话总结 ≤60字",
  "key_takeaways": ["要点1", "要点2", ...],
  "entities": [{"name":"", "type":"concept|person|term", "first_mention":"MM:SS"}],
  "chapters": [{"start":"MM:SS", "end":"MM:SS", "title":"章节标题"}],
  "markdown": "完整 Markdown 报告"
}
```

**硬性约束**：`chapters` 数量必须 ==段落数，时间戳必须与分段一致。

---

## 三、TextTiling 算法详解（面试重点！）

### 是什么

TextTiling 是 Marti Hearst 1997 年提出的经典文本主题分段算法。核心假设：同一话题内词汇重复率高，话题切换时词汇突然变化（相似度谷底）。

### 我的实现（4 步）

```
步骤 1: 向量化
  每条ASR 句子 → tokenize(中文单字+ 英文单词)
  → TF词频统计 → hash(word) % 512 映射到 512 维向量
  → L2 归一化

步骤 2: 余弦相似度
  相邻两句的 512 维向量做点积 = 余弦相似度
  输出: similarities[n-1]

步骤 3: 平滑
  滑动窗口 W=5 做均值平滑，消除单点噪声

步骤 4: 谷底检测 (depth score)
  depth[i] = (mean(smoothed[i-5..i-1]) + mean(smoothed[i+1..i+5])) / 2 - smoothed[i]
  阈值 = mean(depth) + 0.3 * std(depth) + 0.005
  depth > 阈值 且 局部极大 → 判定为边界
```

### 为什么不用 BERT/Sentence-BERT？

> 1. 长视频 ASR 几千句，BERT 编码首屏延迟不可接受
> 2. TextTiling O(n)纯 CPU毫秒级返回
> 3. 中文 ASR 短句噪声大，词汇一致性比语义嵌入更鲁棒
> 4. TF-IDF hash 到 512 维，内存开销极小

### 为什么只用 TF 没用 IDF？

> 语料是单个视频的字幕（单文档），没有跨文档语料库，IDF 无意义。相当于"单文档内的词频对比"，效果足够。后续可以引入通用 IDF 词典优化。

---

## 四、多模态融合设计

### 为什么要融合？

| 单路| 问题 |
|------|------|
| 纯文本 TextTiling | 无语音段（纯画面）无法检测边界 |
| 纯视觉场景标签 | 同一画面下的话题切换检测不到 |

### 融合机制

```
1. 音频边界: [{ts, audioScore=0.8}]
2. 视频边界: [{ts, videoScore=0.9}]
3. 统一到候选列表，乘权重: audio×0.6, video×0.4
4. 时间窗 2s 内的候选合并（加权平均时间戳和分数）
5. 分数阈值 = meanScore × 0.3，低于阈值的去掉
6. 自适应长度: 段< 3s → 合并到前段; 段 > 120s → 中点劈开
```

### 权重 0.6/0.4 怎么来的？

> 实测调参。大部分有台词的视频中，话题切换主要靠语义（文本）驱动，画面切换有时只是镜头角度变了但话题没变。所以文本权重高于视觉。无台词视频降级为固定分段。

---

## 五、并发控制 + 网络设计

### QSemaphore 令牌桶

```cpp
QSemaphore m_concurrencyLimit{5};  // 最多 5 个并发请求

void classifySingleScene(...) {
    if (!m_concurrencyLimit.tryAcquire()) {
        QTimer::singleShot(100ms, retry);  // 非阻塞重试
        return;
    }
    postJson(..., [this](...) {
        m_concurrencyLimit.release();      // 响应后归还
    }, [this](...) {
        m_concurrencyLimit.release();      // 失败也归还
    });
}
```

### VLM vs LLM 分工

| 模型 | 用途 | 输入 |
|------|------|------|
| qwen-vl-plus (VLM) | 帧分析、场景分类 | 图片 + 文本 |
| qwen-plus (LLM) | 最终报告合成 | 纯文本（段落描述汇总） |

**为什么不全用 VLM？**
> 报告生成需要整合所有段落（上下文很长），VLM 的视觉 token 占位会挤压文本窗口。纯文本 LLM 可用更长上下文、更低成本。

---

## 六、缓存设计

### 缓存键

```cpp
SHA256(文件绝对路径 + 文件大小 + 最后修改时间).hex().left(16)
```

### 缓存内容

完整 JSON 包含：`report`(tldr/要点/实体/章节/markdown) + `segments` + `asrResults`

### 命中流程

```
startSummary()
  → tryLoadFromCache(videoPath)
  → 缓存键匹配 → 反序列化 → emit reportChanged() → 直接 Finished
```

### 失效条件

文件路径/大小/修改时间任一变化 → SHA256 不同 → 缓存不命中 → 重新分析。

---

## 七、开发中遇到的问题

### 问题 1：场景标签抖动导致过度分段

**现象**：明明是同一场景的连续镜头，VLM 偶尔返回不同标签（如交替`indoor_meeting` 和 `indoor_talking`），导致每15s 都判定为场景切换，最终切出几十个碎片段。

**原因**：VLM 对相似场景的标签不稳定，单帧判断没有上下文一致性。

**解决**：三级去抖 ——
1. 空标签填补（请求失败帧用相邻标签）
2. 中值滤波 K=2（前后各 2 帧做众数投票，消除孤立异常）
3. 单帧噪声吸收（A-B-A 模式 → A-A-A）

效果：场景切换次数从平均 20+ 降到 5~8 个。

### 问题 2：LLM 返回的章节时间戳与分段不一致

**现象**：给LLM 5个段落，它输出 3 个章节，时间戳也对不上。

**原因**：LLM 倾向于"自作聪明"地合并或重新切分段落。

**解决**：在 System Prompt 中加入**硬性约束**：
```
- chapters 的个数必须等于下方提供的"时间段"个数
- 每个 chapter 的 start/end 必须与对应时间段完全一致
- 不允许拆分/合并时间段
```
并在代码中做fallback：如果返回的 chapters 数量不匹配，用段落本身的时间戳 + description 前20 字作为 title。

### 问题 3：Worker 线程退出后 m_networkClient 的thread affinity 错误

**现象**：第一次总结正常，第二次点击"重新分析"时crash，报"QObject::moveToThread: Current thread is not the object's thread"。

**原因**：`stopSummary()` 中 `delete m_workerThread` 后，`m_networkClient` 的 `thread()` 变成 `nullptr`（Qt 行为：当 QThread 被销毁时，属于该线程的对象的 `thread()` 置空）。

**解决**：在 `delete m_workerThread` 之前，先`m_networkClient->moveToThread(QCoreApplication::instance()->thread())` 把对象移回主线程。`startSummary()` 开头也加双保险检查。

### 问题 4：1h 长视频分析报告超时

**现象**：长视频段落太多（40+ 段），buildSegmentContext拼出的 prompt 超过 LLM 的 context window，返回 400错误。

**原因**：每段 2-3 句描述 × 40 段 + 完整 ASR 文本 → 超过 qwen-plus 的 8K token 限制。

**解决**：在 `buildSegmentContext()` 中做截断——每段描述限制 100 字，ASR 文本只取每段前 200 字作为上下文。总prompt 控制在 6K token 以内。后续可优化为 map-reduce（先分组摘要再汇总），但当前版本未实现。

### 问题 5：TextTiling 对极短视频（<30s）无效

**现象**：短视频只有 3-5 条ASR，TextTiling 的 depth score 数组为空（需要 2×W=10 条句子才能计算）。

**原因**：算法要求 `asrResults.size() >= 2*WINDOW + 3`，短视频不满足。

**解决**：在 `runAnalysis()` 中判断：ASR 条数 < 10 或语义分段开关关闭时，降级为固定时长分段 `segmentByDuration(durationMs, 5000)`。

---

## 八、面试必答题

### Q1：为什么不直接把整个视频帧都发给VLM一次性理解？
> 1. VLM 上下文有限（通常 4~8 张图片max），1h 视频几千帧塞不进去
> 2. 长上下文下VLM 理解质量急剧下降（"lost in the middle"）
> 3. 分段策略让每段只需 2 帧 + 局部文本，VLM 聚焦局部理解，LLM 负责全局归纳

### Q2：TextTiling 和 LLM 分段有什么区别？为什么不直接让 LLM 分段？
> LLM 分段需要先把全部文本塞进去，长视频超出 context window。TextTiling 是 O(n) 的纯算法，无长度限制、无网络延迟、可确定性复现。用它做"粗分段"，LLM 只做"精细命名"。

### Q3：场景分类为什么用 VLM 而不是传统 CV（如直方图/CLIP）？
> 传统 CV 只能给出"画面是否变化"（低语义），VLM 能给出"这是什么场景"（高语义标签）。高语义标签可以做"相同标签=相同话题"的判断，比纯像素差异更准确。代价是网络延迟，但场景分类可以并行（5 并发），总时间可控。

### Q4：如何保证"画面+语音"在最终总结中不割裂？
> 两处融合：
> 1. **分段时融合边界信号**：文本语义谷底 + 视觉场景切换 → 统一边界，保证段落内画面和语义是一致的
> 2. **分析时融合内容**：每段的 2 帧画面 + 该段 ASR 文本同时作为 VLM 的 prompt，让模型"看着画面听着内容"做跨模态理解

### Q5：0.6/0.4 的权重怎么定的？有没有做消融实验？
> 诚实回答：这是手动调参的经验值，在5个测试视频上比对了 0.5/0.5、0.7/0.3、0.6/0.4 三组，0.6/0.4 在"教学类视频"上边界 F1 最高。后续可以做 grid search 或学习权重，但当前数据量不足以训练。

### Q6：缓存键为什么用 路径+大小+mtime 而不是文件内容 hash？
> 文件内容 hash（如 MD5）需要读取整个文件（1h 视频 ≈ 2GB），IO 开销不可接受（几秒到十几秒）。路径+大小+mtime 是"近似唯一标识"，极低成本。缺点是文件被"原地修改但大小和mtime不变"时会误命中——但这种情况在实际使用中几乎不存在。
