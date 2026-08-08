# ASR 字幕链路调试纪实

> 记录 `feature_dev/2.1.0/caption_refactor` 分支中 ASR 架构重构后，字幕从"完全不工作"到"正常运行"的完整调试过程。

## 背景

重构将 ASR 架构从策略模式（`AsrOfflineStrategy`/`AsrRealtimeStrategy`）改为 Pipeline + AudioSource 模式，统一了 VAD→ASR→翻译 的编排流程。重构后字幕完全无法工作，经过多轮排查共发现 19 个问题。

---

## 问题一：悬空信号 + 死代码

**现象**：`AsrManager` 的 `subtitleReady`/`translationReady`/`engineError` 信号从 Pipeline 转发，但全项目无人 connect。`playercore.h` 声明了 `onSubtitleReady(SubtitleItem)` 但无实现、无 connect。

**根因**：最初设计为"信号驱动"渲染，后因线程安全（`std::thread` 非 QThread）改为时钟轮询，但未清理信号代码。

**修复**：
- 删除 `playercore.h` 中 `onSubtitleReady` 死代码声明
- `AsrManager` 的信号 connect 改为 `Qt::QueuedConnection`，确保从 `std::thread` emit 的信号安全投递到主线程

---

## 问题二：翻译功能完全失效

**现象**：开启翻译后渲染的仍是单语（只有原文），翻译完全不生效。

**根因**：两个 bug 叠加：

1. **译文被队列去重丢弃** — `SubtitleQueue::mergeBatch` 的去重 key = `"start|end|text"`，不含 `translated_text`。译文 push 时 key 与原文相同，被 `seen_` 判定为重复直接丢弃。

2. **译文更新不触发重绘** — `checkAndUpdateSubtitle` 只比较 `sub.text != current_display_sub_.text`。译文异步回填后原文没变，条件为 false，不重新渲染。

**修复**：
- `mergeBatch`：key 命中时若新条目携带 `translated_text`，回填到 `cache_` 中已有记录
- `checkAndUpdateSubtitle`：比较条件加入 `translated_text` 变化检测

---

## 问题三：打开文件时 ASR 引擎类型未加载

**现象**：config.ini 设置 `engineType=1`（SenseVoice），但打开文件后日志显示用 Whisper API 加载 SenseVoice 模型目录。

**根因**：`openInternal()` 在调用 `asr_manager_->init()` 前没有加载 ASR 配置，`asr_engine_type_` 保持枚举默认值 `Whisper`（=0）。而 `setAsrEnabled()` 和 `setTranslationEnabled()` 路径有配置加载，所以通过 UI 开关触发时不会出问题。

**修复**：提取 `applyAsrConfig()` 公共方法，三个 init 入口（`openInternal`/`setAsrEnabled`/`setTranslationEnabled`）统一调用，消除重复代码。

---

## 问题四：AsrModelCache 误用 Whisper API 加载 SenseVoice 模型

**现象**：日志出现 `whisper_init_from_file_with_params_no_state: failed to open 'models/asr/sensevoice'`。

**根因**：`AsrModelCache` 是 Whisper 专用缓存，但 `setModelPath()`/`warmUp()` 无条件触发，不管当前引擎是什么。且 `setAsrEnabled()` 中先调用 `setModelPath()` 再调用 `applyAsrConfig()`，此时 `asr_engine_type_` 还是默认 `Whisper`。

**修复**：
- `AsrManager::setModelPath()`/`warmUp()` 加条件：`if (asr_engine_type_ == Whisper)` 才触发缓存
- `setAsrEnabled()` 调换顺序：先 `applyAsrConfig()`（设引擎类型）再 `setModelPath()`

---

## 问题五：SenseVoice 推理崩溃 — ONNX 输入数量不匹配

**现象**：`abort() has been called`，异常在 `onnxruntime_cxx_inline.h:37`。

**根因**：SenseVoice ONNX 模型有 **4 个输入**（`speech`/`speech_lengths`/`language`/`textnorm`），但 `recognize()` 只提供了 **2 个**（speech + speech_lengths），`session->Run()` 参数不匹配 → ONNX 抛异常 → 未捕获 → `abort()`。

**修复**：
- `loadModel` 按输入名记录各输入索引
- `recognize` 按索引顺序构造全部 4 个输入张量（`language=0` 自动语言识别，`textnorm=15` 带标点反归一化）
- `session->Run()` 加 try-catch 捕获 `Ort::Exception`

---

## 问题六：FSMN-VAD 推理崩溃 — 输入数量不匹配 + 输入类型错误

**现象**：开启字幕后 `abort()` 崩溃。日志显示 `n_inputs=5 n_outputs=5`。

**根因**：FSMN-VAD 模型有 **5 个输入**（`speech` + 4 个 `in_cache`）和 5 个输出，但原代码：
1. 只提供 **2 个输入**（speech + 1 个 cache）
2. **speech 传的是原始 PCM**，而模型要的是 **400 维 FBank+LFR+CMVN 特征**
3. `session->Run()` 无 try-catch

**修复**：重写 `FsmnVad`：
- 新增 `computeFeatures()`：PCM → FBank(80维) → LFR(m=5,n=1) → 400维 → CMVN
- 按模型声明**动态分配全部 cache**（不硬编码个数/shape），每次推理后从输出回写
- `inferFeatures()` 加 try-catch，失败时 `ready_=false` 降级到 no-VAD 模式
- 逐帧 softmax 得概率（原代码每个 chunk 只取最后一帧，时间轴偏差 16 倍）

---

## 问题七：loadCmvn 不支持 Kaldi am.mvn 格式

**现象**：日志 `CMVN loaded: mean_dim= 0 var_dim= 0`。

**根因**：`am.mvn` 是 Kaldi nnet 文本格式（`<AddShift>`/`<Rescale>` 标记 + `[ ... ]` 数值块），原代码按"两行纯数字"解析，把 `<Splice>` 后的 `[ 0 ]` 当成了 mean。

**修复**：按 `<AddShift>`/`<Rescale>` 标记定位数值块，跳过 `<Splice>` 干扰。同时 `FsmnVad` 校验 CMVN 维度与 `feat_dim` 一致才启用。

---

## 问题八：开启字幕瞬间卡顿 2-3 秒

**现象**：点击字幕开关后 UI 冻结 2-3 秒。

**根因**：两个 ONNX 模型（VAD ~60MB + SenseVoice ~200MB）在**主线程**同步加载，Debug 构建下 ONNX Runtime 图优化 + 内存分配极慢。

**修复**：
- 新增 `AsrManager::initAsync()`，模型加载在工作线程完成
- `init_mtx_` 保护 `pipeline_` 创建/销毁，`init_cancelling_` 支持快速取消
- 三个调用点全部改用 `initAsync`

---

## 问题九：离线识别全速跑导致播放卡顿

**现象**：开启字幕后播放卡顿，尤其从视频开头开始时。

**根因**：`offlineLoop` 无节流地全速识别整个文件，叠加 Debug 构建 ONNX 极慢 + 多个 intra-op 线程抢占解码/渲染线程。

**修复**：自适应节流策略（Adaptive Throttling）：
- **追赶阶段**（识别落后播放）：yield 2ms，靠 `THREAD_PRIORITY_BELOW_NORMAL` 保护渲染
- **跟随阶段**（已追上）：yield = chunk 时长 - 处理耗时的 70%，匀速推进
- **超前阶段**（超前 20s）：精确等待，几乎不消耗 CPU

---

## 问题十：引擎每次打开视频都重新加载

**现象**：切换文件时日志显示重新加载 VAD + ASR 模型，等待 2-3 秒。

**根因**：`Pipeline` 拥有引擎（`unique_ptr`），每次 `init()` 都 `createVadEngine()` + `createAsrEngine()` 加载模型，`stop()` 时 `pipeline_.reset()` 释放。

**修复**：引擎生命周期从 Pipeline 剥离到 AsrManager：
- `AsrManager` 缓存 `cached_vad_`/`cached_asr_`/`cached_translator_`，仅在引擎类型或模型路径变化时重建
- Pipeline 通过裸指针引用引擎，不拥有
- `stop()` 只停 Pipeline 线程 + 清队列，保留引擎缓存
- `releaseEngines()` 供程序退出时调用

---

## 问题十一：FBank 特征偏差 — Mel 滤波器组 + FFT 补零

**现象**：VAD 的 `max_prob` 持续在 0.5 附近，模型无法区分语音和静音。

**根因**：FBank 实现与 FunASR 训练时的特征提取不一致：

1. **Mel 滤波器组中心点计算错误** — 用 `n_filters` 等分而非标准 `n_filters+1` 等分（`n_filters+2` 个点），滤波器覆盖范围和中心频率全部偏移
2. **n_fft = 400**（等于 frame_length）而非标准 512（补零到 2 的幂次）
3. **Mel 权重有 +1 偏差** — `(k-left)/(center-left+1)` 而非 `(k-left)/(center-left)`，特征能量被压缩
4. **dither=0** — 静音段特征全零，与训练分布不符
5. **预加重第一帧 `*0.03`** — 压缩第一帧能量

**修复**：
- Mel 滤波器组改用标准 `n_filters+2` 等间距点
- `n_fft` 改为 512，DFT 补零
- Mel 权重去掉 `+1`
- `dither` 默认改为 1.0
- 预加重第一帧不做处理

---

## 问题十二：VAD 状态机振荡

**现象**：同一个 chunk 内反复触发 `speech start`，语音段被切碎成 100-200ms 碎片，全部被 `min_speech_ms` 过滤，`vad_segments=0`。

**根因**：单一阈值 0.5，概率在 0.499-0.501 之间逐帧波动，导致状态机在 `Silence ↔ Speech ↔ Trailing` 之间高频振荡。

**修复**：
- **迟滞阈值**：`Silence→Speech` 用高阈值（0.5），`Speech/Trailing→Silence` 用低阈值（0.3）
- **概率滑动平均**：3 帧窗口平滑，减少逐帧波动
- **Speech 状态持续更新 end 时间**：原代码只在退出 Speech 时设 end，导致短段被截断

---

## 问题十三：FSMN-VAD 输出解析错误 — 248 类只用 2 类做二分类

**现象**：即使 FBank 修复后，`max_prob` 仍在 0.5 附近，VAD 检测不到语音。

**根因**：FSMN-VAD 模型输出维度是 **248 类**声学 PDF（不是 2 类），配置中 `sil_pdf_ids=[0]`。原代码只取 class 0 和 class 1 做二分类 Softmax，导致概率永远在 0.5 附近。

**修复**：对全部 248 个输出类别执行 Softmax，按模型配置计算：
```
P(speech) = 1 - P(silence_class0)
```

---

## 问题十四：跨块语音段 PCM 截取错误

**现象**：VAD 返回 `3.41-11.26s` 的段，但该结果在 10s chunk 才返回，代码只能从当前 10-20s PCM 中截取，实际只传给 SenseVoice 10-11.26s（20159 个采样而非完整的 125600 个）。

**根因**：VAD 是流式的，语音可能在后续 10 秒块中才结束。原代码从**当前 chunk 的 PCM** 中截取 VAD 段，跨块段被截断。更严重的情况如 `13.78-70.22s`（56 秒）：最终只传入 70-70.22s 的尾巴，SenseVoice 收到的几乎不是完整语音。

**修复**：
- 增加 `vad_pcm_buffer_` 连续 PCM 历史缓冲，VAD 段从历史缓冲中完整截取
- 时间戳不连续检测 + VAD/缓冲 reset
- 重叠 PCM 跳过，防止重复送入 VAD
- 30s 缓冲上限防止内存泄漏
- `max_speech_ms = 20000`：连续语音达到 20 秒强制切段，避免超长 ASR 输入
- EOF 时 `vad_->flush()` 处理最后一个未闭合段
- `stop()`/`reset()` 清理 PCM 历史和 VAD 状态

---

## 架构总结

### 实时字幕链路（修复后）

```
入口：打开文件 / UI 开关
  ↓
PlayerCore::openInternal / setAsrEnabled
  ↓ applyAsrConfig() 加载引擎/VAD/翻译配置
  ↓
AsrManager::initAsync()           ← 异步，不阻塞主线程
  ├─ 引擎缓存检查（跨文件复用）
  ├─ init() → 创建音频源 → 创建 Pipeline → 注入引擎指针
  └─ start() → 启动 offlineLoop 线程
  ↓
AsrPipeline::offlineLoop          ← 工作线程，BELOW_NORMAL 优先级
  ├─ 自适应节流（追赶/跟随/超前三阶段）
  ├─ processVadAudio()
  │   ├─ 追加 PCM 到历史缓冲
  │   ├─ VAD process() → FBank(80) → LFR(m=5) → CMVN → ONNX 推理
  │   │   └─ 248 类 Softmax → P(speech) = 1 - P(silence)
  │   ├─ 迟滞阈值状态机 + 概率平滑 + max_speech_ms 强制切段
  │   └─ processVadSegments() → 从历史缓冲完整截取 PCM
  ├─ SenseVoice ASR: FBank(80) → LFR(m=7) → 560维 → ONNX 推理
  │   └─ 4 输入(speech/lengths/language/textnorm) → CTC 解码
  ├─ queue_->push(item)
  └─ translateLoop() → 异步翻译 → queue_ 译文回填
  ↓
PlayerCore::checkAndUpdateSubtitle()  ← 渲染线程每帧轮询
  ├─ queue_->getCurrent(now) → 查找当前时间的字幕
  └─ text 或 translated_text 变化 → emit subtitleReady → OpenGL 渲染
```

### 关键设计决策

| 决策 | 理由 |
|------|------|
| Pipeline 通过裸指针引用引擎 | 引擎跨文件复用，不随 Pipeline 重建 |
| initAsync 异步加载 | ONNX 模型加载 2-3s，不能阻塞主线程 |
| 自适应节流 | 全速识别占满 CPU 导致播放卡顿 |
| 迟滞阈值 + 概率平滑 | 单一阈值导致状态机振荡 |
| 248 类完整 Softmax | 模型输出不是二分类，sil_pdf_ids=[0] |
| PCM 历史缓冲 | VAD 流式输出跨块段，需要完整截取 |
| max_speech_ms=20s | 防止超长段导致 ASR 输入过大失败 |

### 涉及文件

| 文件 | 修改内容 |
|------|---------|
| `src/subtitle/asrmanager.h/.cpp` | 引擎缓存、initAsync、applyAsrConfig |
| `src/subtitle/asrpipeline.h/.cpp` | PCM 历史缓冲、自适应节流、引擎指针引用 |
| `src/subtitle/fsmnvad.h/.cpp` | FBank+LFR+CMVN 前处理、248类解析、迟滞阈值、max_speech_ms |
| `src/subtitle/sensevoiceengine.h/.cpp` | 4 输入构造、FBank+LFR 特征提取、try-catch |
| `src/subtitle/ivadengine.h` | threshold_exit、max_speech_ms、smoothing_window |
| `src/queue/subtitlequeue.h` | 译文回填去重 |
| `src/utils/fbank.h` | Mel 滤波器组、FFT 补零、dither、预加重 |
| `src/utils/onnxruntimeutil.h` | Kaldi am.mvn 解析 |
| `src/core/playercore.h/.cpp` | applyAsrConfig、initAsync 调用、播放位置 provider |

---

## 问题十五：开启翻译时卡住主线程

**现象**：字幕已开启后，点击"中英翻译"开关，主线程卡死数秒。

**根因**：`PlayerCore::setTranslationEnabled` 在 ASR 运行时切换翻译开关，会执行 `asr_manager_->stop()` + `applyAsrConfig()` + `initAsync()`——即**停止并重建整个 ASR Pipeline**（VAD + ASR + 翻译 + 音频源）。其中 `stop()` 会 `join` 异步初始化线程和 VAD/ASR 线程，主线程被长时间阻塞。而 `AsrPipeline` 本身已有 `enableTranslation()` 方法可以动态启停翻译线程，无需重建整个管线。

**修复**：
- 新增 `AsrManager::applyTranslationToggle()`：开启翻译时懒加载翻译引擎（若未缓存）并注入 Pipeline，然后启动翻译线程；关闭翻译时仅设标志通知翻译线程退出，不 `join`
- `PlayerCore::setTranslationEnabled` 改为调用 `applyTranslationToggle()`，不再 `stop + reinit`
- `AsrPipeline::enableTranslation(false)` 仅设 `translate_running_=false` 并 `notify`，不 `join`——翻译线程在当前 HTTP 请求返回后自行退出，`join` 延迟到下次开启或 `stop()` 时

---

## 问题十六：字幕/翻译开关切换视频后状态丢失

**现象**：开启字幕和翻译开关后切换视频，两个开关恢复为关闭状态，不自动恢复。

**根因**：
1. 字幕开关 `realtimeBtn_` 的 `toggled` 信号只调用 `player_->setAsrEnabled(on)`，**不持久化到配置**；切换视频时 `PlayerCore::stop()` 停止了 ASR，新视频打开后不恢复
2. 翻译开关虽持久化到配置，但创建 `SubtitlePopup` 时**不从配置恢复按钮 UI 状态**；切换视频后也不自动恢复翻译
3. `applyPersistentSettings()` 只恢复倍速/音量/静音，**不恢复字幕和翻译**
4. `PlayerCore::stop()` 不重置 `asrEnabled_`，导致 `PlayerViewModel::setAsrEnabled` 的去重逻辑（`if (isAsrEnabled() == enabled) return`）跳过重新初始化

**修复**：
- `ConfigManager` 新增 `getAsrEnabled()`/`setAsrEnabled()` 持久化字幕开关到 `asr/enabled`
- `MainWindow::on_subtitleBtn_clicked` 创建 `SubtitlePopup` 时，在 `connect` 信号**之前**调用 `setChecked` 从配置恢复两个开关状态（避免 `toggled` 误触发），并手动同步图标
- `realtimeBtn_` toggled 时新增 `ConfigManager::setAsrEnabled(on)` 保存状态
- `applyPersistentSettings()` 添加字幕和翻译开关恢复逻辑
- `PlayerCore::stop()` 添加 `asrEnabled_ = false`，确保切换视频后 `setAsrEnabled(true)` 不被去重跳过

---

## 问题十七：seek 后字幕中断——FileAudioSource 独立 Demuxer 未 seek

**现象**：seek 之后一段时间无法识别到字幕，过了一会儿才会有。

**根因**：`FileAudioSource` 有自己独立的 `Demuxer` + `Decoder`。`PlayerCore::seek()` 只 seek 了播放器的 demuxer，调用的 `asr_manager_->reset()` 只清空了 VAD/ASR 内部缓冲，**没有 seek `FileAudioSource` 的独立 demuxer**。

导致 seek 后 ASR 继续从旧位置读音频：
- ASR 识别的音频时间戳对应旧位置，`SubtitleQueue` 中找不到匹配当前播放位置的字幕
- 要等 `FileAudioSource` 从旧位置读到新位置后才恢复（几秒到几十秒）

**修复**：
- `IAudioSource` 新增 `seekTo(double pos_sec)` 接口
- `FileAudioSource::seekTo`：seek 内部 demuxer + flush decoder + 清空 PCM 缓冲 + 重置时间
- `LiveAudioSource::seekTo`：清空 ring buffer（Push 模式新帧由外部正确位置送入）
- `AsrPipeline::seekTo`：seek 音频源 + reset VAD/ASR + 清空翻译队列 + 清空字幕队列
- `AsrManager::seekTo`：透传到 pipeline
- `PlayerCore::seek` 改用 `asr_manager_->seekTo()` 替代 `reset()`

---

## 问题十八：seek 后字幕比音频慢（时间戳不精确）

**现象**：seek 后字幕显示比音频慢，存在明显不同步。

**根因**：`FileAudioSource::fillBuffer` 解码出的 `frame` 有真实的 `pts`（时间戳），但被忽略了。`pull` 方法用 `current_time_sec_` 累加方式估算时间——seek 后 `current_time_sec_ = pos_sec`（目标位置），但 demuxer 实际 seek 到的是最近关键帧，可能偏前几秒。导致 ASR 拿到的 `media_time` 比实际音频位置偏大，字幕时间戳超前于实际播放位置。

**修复**：
- `FileAudioSource` 新增 `audio_tb_` 成员保存音频流时间基
- `fillBuffer` 中用 `frame->pts * av_q2d(audio_tb_)` 校准 `current_time_sec_`，每帧都用真实 pts，不再靠累加估算
- `seekTo` 中重置 `time_calibrated_` 标志，等待 `fillBuffer` 用 pts 重新校准

---

## 问题十九：seek 后字幕恢复延迟

**现象**：seek 后不会立即识别到语音，还是会等一小会。

**根因**：seek 后 `offlineLoop` 的自适应节流策略仍会 yield——即使 `ahead_sec ≈ 0`（追赶阶段），跟随阶段仍会 yield `spare_ms * 0.7`，以及超前等待可能误判。导致 ASR 首个 chunk 处理被延迟。

**修复**：
- `AsrPipeline` 新增 `seek_flag_` 原子标志
- `seekTo` 中设置 `seek_flag_ = true`
- `offlineLoop` 下一次迭代检查 `seek_flag_`，若为 true 则跳过超前等待和节流 yield，让 ASR 立即处理首个 chunk 尽快产出字幕
- `seek_flag_` 使用 `exchange(false)` 确保只生效一次
