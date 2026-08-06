基于当前代码实现状态，以下是完整的 TODO 清单：

---

## TODO 清单

### 一、依赖引入

| # | 任务 | 优先级 | 说明 |
|---|------|--------|------|
| 1 | 引入 ONNX Runtime 库 | P0 | FSMN-VAD 和 SenseVoice 都依赖它 |
| 2 | 引入 CTranslate2 库 | P1 | NLLB / MarianMT 本地翻译推理需要 |
| 3 | 引入 SentencePiece 库 | P1 | NLLB / MarianMT 的分词器 |
| 4 | CMakeLists.txt 添加 onnxruntime 查找和链接 | P0 | `FindOnnxRuntime.cmake` |

---

### 二、VAD 模块

| # | 任务 | 文件 | 说明 |
|---|------|------|------|
| 5 | `fsmnvad.cpp` — 替换能量占位为真实 ONNX 推理 | `fsmnvad.cpp` | 当前 `inferFrame()` 是能量阈值占位 |
| 6 | 下载/集成 FSMN-VAD ONNX 模型 | 资源 | FunASR 导出的 `fsmn_vad.onnx` |
| 7 | 确定 FSMN 隐藏状态 cache 维度 | `fsmnvad.h` | 当前 `cache_.resize(128*4)` 是占位 |
| 8 | 实现 Silero VAD 备选引擎 | 新文件 | `silerovad.h/.cpp` |

---

### 三、ASR 引擎

| # | 任务 | 文件 | 说明 |
|---|------|------|------|
| 9 | `sensevoiceengine.cpp` — 实现完整推理 | `sensevoiceengine.cpp` | FBank特征提取 → ONNX encoder → CTC解码 |
| 10 | SenseVoice 模型文件集成 | 资源 | `encoder.onnx` + `tokenizer` |
| 11 | SenseVoice 输出后处理（去标签） | `sensevoiceengine.cpp` | 去除 `<\|语言\|><\|情感\|>` 标签 |
| 12 | `cloudasrengine.cpp` — 完善腾讯云签名认证 | `cloudasrengine.cpp` | 当前签名是占位 |
| 13 | CloudASR 支持阿里云/Azure 多 provider | `cloudasrengine.cpp` | 按 `cloud_cfg_.provider` 分发 |
| 14 | CloudASR 时间戳对齐优化 | `cloudasrengine.cpp` | 当前 `end_sec = base_sec + 30.0` 是粗估 |

---

### 四、翻译引擎

| # | 任务 | 文件 | 说明 |
|---|------|------|------|
| 15 | `nllbtranslator.cpp` — 实现 CTranslate2 推理 | `nllbtranslator.cpp` | tokenize → translate → detokenize |
| 16 | `marianmttranslator.cpp` — 实现 CTranslate2 推理 | `marianmttranslator.cpp` | 同上 |
| 17 | `tencenttranslator.cpp` — 修复 TC3-HMAC-SHA256 签名 | `tencenttranslator.cpp` | 当前签名拼接缺少空格 |
| 18 | GPT 翻译支持自定义 model 字段 | `gpttranslator.cpp` | 当前硬编码 `gpt-4o-mini` |
| 19 | 翻译批量接口优化（腾讯云 TextTranslateBatch） | `tencenttranslator.cpp` | 当前逐条翻译 |

---

### 五、管线整合 (AsrPipeline)

| # | 任务 | 文件 | 说明 |
|---|------|------|------|
| 20 | AsrPipeline 离线模式集成 | `asrofflinestrategy.cpp` | 策略内部改用 `AsrPipeline::feedPcm()` |
| 21 | AsrPipeline 实时模式集成 | `asrrealtimestrategy.cpp` | 策略内部改用 `AsrPipeline::feedAudio()` |
| 22 | 翻译结果回写字幕队列去重 | `asrpipeline.cpp` | 避免译文重复 push 到 queue |
| 23 | VAD 段跨窗口拼接处理 | `asrpipeline.cpp` | 实时模式下 VAD 段可能跨越滑动窗口边界 |

---

### 六、UI 层适配

| # | 任务 | 文件 | 说明 |
|---|------|------|------|
| 24 | 字幕弹窗显示双语（原文+译文） | `subtitlepopup.h/.cpp` | 读取 `SubtitleItem::translated_text` |
| 25 | 设置面板：ASR 引擎选择下拉框 | UI | Whisper / SenseVoice / Cloud |
| 26 | 设置面板：翻译引擎选择 + 开关 | UI | GPT / NLLB / MarianMT / 腾讯翻译 |
| 27 | 设置面板：VAD 开关 | UI | 启用/禁用 |
| 28 | 设置面板：API Key 输入（翻译/云ASR） | UI | 存储到本地配置 |

---

### 七、测试与质量

| # | 任务 | 说明 |
|---|------|------|
| 29 | 验证 WhisperEngine 兼容性 | 确保重构后现有功能不回退 |
| 30 | 引擎热切换稳定性测试 | 播放中切换 ASR/翻译引擎 |
| 31 | 内存泄漏检查 | 引擎反复 init/release 场景 |
| 32 | 翻译线程安全验证 | 高频 ASR 输出 → 翻译队列堆积场景 |

---

**推荐执行顺序**：1→5→6→7→9→10→20→21→24→29 → 其余按优先级