# FSMN-VAD 问题排查与修复报告

## 问题现象

实时字幕功能中，FSMN-VAD 语音活动检测模块出现严重故障：

1. **VAD 概率恒定在 0.99**：无论有无人声，输出的语音概率始终维持在 0.990~0.995
2. **无法检测静音间隔**：句子之间的停顿完全检测不到
3. **只能靠强制切段**：每 20 秒（`max_speech_ms`）被迫切断一次
4. **SenseVoice 识别失败**：`decoded tokens contain no speech text`

**典型日志表现**：
```
[FsmnVad] speech prob @ 3 s prob= 0.995382
[FsmnVad] speech prob @ 4 s prob= 0.990225
[FsmnVad] speech prob @ 5 s prob= 0.991638
[FsmnVad] max-length segment @ 0 - 8 s
[AsrPipeline] VAD audio @ 0 s samples= 160000 segments= 1
[SenseVoice] decoded tokens contain no speech text: token_count= 4
[AsrPipeline] ASR recognize failed for seg 0
```

---

## 问题根源（多重错误叠加）

经过系统性诊断，发现**三个层次的问题**同时存在：

### 1. 跨块语音段截取错误（已在前期修复）

**问题**：VAD 是流式的，语音段可能跨越多个 10 秒块才闭合。例如 `3.41–11.26s` 的语音段在 10 秒块才返回，但流水线只从当前块（`10–20s`）截取 PCM，实际只传给 SenseVoice `10–11.26s` 的尾巴（20159 个采样），而不是完整的 7.85 秒（125600 个采样）。

**修复**：
- 在 `AsrPipeline` 中维护 30 秒 PCM 历史缓冲（`vad_pcm_buffer_`）
- VAD 段跨块时从历史缓冲提取完整音频
- 检测时间戳不连续时自动重置

**代码位置**：`src/subtitle/asrpipeline.{h,cpp}`

---

### 2. FBank 能量计算异常（核心问题 1）

**问题**：最初 `pcm_scale = 1.0`，但 FunASR 的 CMVN 参数是基于 int16 幅度范围训练的。

**诊断过程**：
```
[FsmnVad] PCM input range: min= -1.31 max= 1.42 abs_max= 1.42
[FsmnVad] Feature before CMVN: min= -23.03 max= 6.40
[FsmnVad] Feature after CMVN: min= -5.74 max= -1.02  ← 全是负值！
```

**根因分析**：
- 重采样器输出归一化浮点 PCM：`[-1.42, 1.31]`
- `pcm_scale=1.0` 时信号幅度太小
- FFT 功率谱能量极低
- `log(能量)` 全是大负值（-23 ~ 6）
- CMVN 归一化后仍然全是负值（-5.74 ~ -1.02）
- VAD 模型收到的特征分布与训练时完全不匹配

**修复**：
```cpp
// src/utils/fbank.h
struct Config {
    float pcm_scale = 32768.0f;  // 归一化输入 [-1,1] → int16 幅度范围
};
```

修复后特征范围正常：
```
[FsmnVad] Feature after CMVN: min= -5.74 max= 2.00  ← 有正有负！
```

---

### 3. 模型输出后验概率解析错误（核心问题 2）

**问题**：错误地对模型输出做了 softmax，并且语义理解反了。

**诊断过程**：
```
[FsmnVad] Model output: T= 64 C= 248
[FsmnVad] First frame posterior[0:10]: 0.402784 0.000234 0.000425 ...
[FsmnVad] Frame 0: silence_prob= 0.006 speech_prob= 0.994
```

**关键发现**：
1. **模型输出已经是概率**：
   - `posterior[0] = 0.403`（最大）
   - `posterior[1-9] = 0.002~0.0007`
   - 所有值加起来接近 1.0
   - FSMN-VAD 量化模型已包含 softmax 层，**不需要再做 softmax**

2. **语义理解错误**：
   - 原实现：`speech_prob = 1 - P(class_0)`
   - 导致：`class_0` 概率高时反而被认为是语音
   - 实际上：FunASR 中 `sil_pdf_ids=[0]` 表示 **class_0 = 静音**
   - 正确逻辑：`speech_prob = 1 - posterior[0]`（直接取反即可）

**错误代码**（已修复前）：
```cpp
// 错误：对已经是概率的输出再做 softmax
const double silence_prob = std::exp((double)row[0] - max_logit) / denominator;
probs[t] = std::clamp((float)(1.0 - silence_prob), 0.0f, 1.0f);  // 逻辑还反了
```

**正确代码**：
```cpp
// 直接使用模型输出的后验概率
const float silence_posterior = row[0];
probs[t] = std::clamp(1.0f - silence_posterior, 0.0f, 1.0f);
```

**代码位置**：`src/subtitle/fsmnvad.cpp:inferFeatures()`

---

## 完整修复方案

### 修改文件清单

| 文件 | 修改内容 |
|-----|---------|
| `src/subtitle/ivadengine.h` | 新增 `max_speech_ms` 配置（20 秒强制切段） |
| `src/subtitle/fsmnvad.{h,cpp}` | ① PDF 解析修正（直接使用后验概率）<br>② 最长语音段限制<br>③ 诊断日志（PCM/特征/模型输出） |
| `src/subtitle/asrpipeline.{h,cpp}` | ① PCM 历史缓冲（30 秒窗口）<br>② 跨块完整音频提取<br>③ EOF 时 flush VAD 残余<br>④ 状态同步清理 |
| `src/utils/fbank.h` | ① `pcm_scale = 32768.0`（匹配 CMVN 训练条件）<br>② `n_fft_ = 512` 固定（与模型配置一致） |

### 关键代码片段

#### 1. PCM 历史缓冲（解决跨块问题）
```cpp
// src/subtitle/asrpipeline.cpp
void AsrPipeline::processVadAudio(const std::vector<float>& pcm, double base_sec) {
    constexpr int SR = 16000;
    constexpr double kTimestampToleranceSec = 0.02;
    
    // 检测时间戳不连续，重置 VAD
    if (vad_pcm_initialized_) {
        const double buffered_end = vad_pcm_base_sec_ + 
            (double)vad_pcm_buffer_.size() / SR;
        const double delta = base_sec - buffered_end;
        if (delta > kTimestampToleranceSec) {
            qWarning() << "[AsrPipeline] VAD audio discontinuity:" << delta << "s";
            vad_->reset();
            vad_pcm_buffer_.clear();
            vad_pcm_base_sec_ = base_sec;
        }
    }
    
    // 拼接新音频到历史缓冲
    vad_pcm_buffer_.insert(vad_pcm_buffer_.end(), new_audio.begin(), new_audio.end());
    
    // VAD 处理
    auto segments = vad_->process(new_audio, new_audio_base);
    processVadSegments(segments);
    
    // 保留 30 秒历史（FSMN 配置最长段 20 秒）
    constexpr size_t kMaxBufferedSamples = 30 * SR;
    if (vad_pcm_buffer_.size() > kMaxBufferedSamples) {
        const size_t remove_count = vad_pcm_buffer_.size() - kMaxBufferedSamples;
        vad_pcm_buffer_.erase(vad_pcm_buffer_.begin(),
                              vad_pcm_buffer_.begin() + remove_count);
        vad_pcm_base_sec_ += (double)remove_count / SR;
    }
}
```

#### 2. 正确解析模型输出（核心修复）
```cpp
// src/subtitle/fsmnvad.cpp
std::vector<float> FsmnVad::inferFeatures(const std::vector<float>& flat_feats, int num_frames) {
    // ... ONNX 推理 ...
    
    const float* probs_data = outputs[0].GetTensorData<float>();
    
    std::vector<float> probs(T);
    for (int t = 0; t < T; ++t) {
        const float* row = probs_data + (size_t)t * C;
        // 直接使用模型输出的后验概率（已包含 softmax）
        // sil_pdf_ids=[0] 表示 class_0 = 静音
        const float silence_posterior = row[0];
        probs[t] = std::clamp(1.0f - silence_posterior, 0.0f, 1.0f);
    }
    return probs;
}
```

#### 3. FBank 幅度匹配 CMVN 训练条件
```cpp
// src/utils/fbank.h
struct Config {
    float pcm_scale = 32768.0f;  // 归一化输入 → int16 幅度范围
};

// extract() 中
for (int i = 0; i < cfg_.frame_length; ++i) {
    frame[i] = pcm[start + i] * cfg_.pcm_scale;  // 放大到 int16 范围
}
```

---

## 诊断方法论

本次排查采用**分层诊断**策略，逐层验证数据流：

```
输入 PCM → FBank 特征 → CMVN 归一化 → 模型推理 → 概率解析 → 状态机
```

### 诊断日志设计

```cpp
// 1. PCM 输入范围
qDebug() << "[FsmnVad] PCM input range: min=" << pcm_min 
         << "max=" << pcm_max << "abs_max=" << pcm_abs_max;

// 2. FBank 特征范围（CMVN 前后）
qDebug() << "[FsmnVad] Feature before CMVN: min=" << before_min << "max=" << before_max;
qDebug() << "[FsmnVad] CMVN params: mean[0:5]=" << ...;
qDebug() << "[FsmnVad] Feature after CMVN: min=" << after_min << "max=" << after_max;

// 3. 模型输出验证
qDebug() << "[FsmnVad] Model output: T=" << T << "C=" << C;
qDebug() << "[FsmnVad] First frame posterior[0:10]:" << ...;
float sum = 0.0f;
for (int c = 0; c < C; ++c) sum += probs_data[c];
qDebug() << "[FsmnVad] prob sum=" << sum << "(should be ~1.0 if softmax applied)";

// 4. 概率计算验证
qDebug() << "[FsmnVad] Frame 0: silence_posterior=" << silence_posterior 
         << "speech_prob(1-sil)=" << probs[t];
```

### 预期正常值

| 阶段 | 预期范围 | 异常表现 |
|-----|---------|---------|
| PCM 输入 | `[-1.5, 1.5]` | `[-32768, 32768]`（未归一化） |
| FBank（CMVN前） | `[-30, 30]` | 全负值或全正值 |
| FBank（CMVN后） | `[-5, 5]`（有正有负） | `[-5.74, -1.02]`（全负） |
| 模型输出和 | `≈ 1.0` | `>> 1.0` 或 `<< 1.0` |
| silence_posterior | `0.1~0.9`（动态） | 恒定 `0.99` 或 `0.01` |
| speech_prob | `0.1~0.9`（动态） | 恒定不变 |

---

## 技术要点总结

### 1. FunASR FSMN-VAD 模型特性

- **输出格式**：`(1, T, 248)` 后验概率（已包含 softmax）
- **语义约定**：`sil_pdf_ids=[0]` → `posterior[0]` 表示静音概率
- **VAD 公式**：`speech_prob = 1 - posterior[0]`
- **量化模型**：`model_quant.onnx` 已将 softmax 融入计算图

### 2. FBank 特征提取要点

- **PCM 缩放**：必须匹配 CMVN 训练时的幅度范围（int16: ±32768）
- **FFT 大小**：固定 512（与 `config.yaml` 一致）
- **CMVN 公式**：`(feature + mean) * variance`
- **预期分布**：归一化后接近标准正态 `N(0, 1)`

### 3. 流式 VAD 的跨块处理

- **缓冲深度**：≥ `max_speech_ms` + 静音拖尾（建议 30 秒）
- **时间对齐**：每次送入时检测时间戳连续性
- **状态同步**：`stop()`、`reset()`、`enableVad()` 时清空缓冲和 VAD 状态
- **EOF 处理**：调用 `vad_->flush()` 输出最后一段

---

## 修复效果验证

### 修复前
```
[FsmnVad] speech prob @ 0 s prob= 0.993899
[FsmnVad] speech prob @ 1 s prob= 0.995989
[FsmnVad] speech prob @ 2 s prob= 0.995982
[FsmnVad] max-length segment @ 0 - 8 s
[AsrPipeline] seg 0 @ 3.41 - 11.26 s pcm= 20159 samples  ← 不完整！
[SenseVoice] decoded tokens contain no speech text
```

### 修复后（预期）
```
[FsmnVad] speech prob @ 0 s prob= 0.05   ← 静音段
[FsmnVad] speech prob @ 1 s prob= 0.82   ← 语音段
[FsmnVad] speech prob @ 2 s prob= 0.87
[FsmnVad] speech prob @ 3 s prob= 0.15   ← 句子间停顿
[FsmnVad] segment @ 1.2 - 2.8 s dur= 1600 ms
[AsrPipeline] seg 0 @ 1.2 - 2.8 s pcm= 25600 samples  ← 完整！
那么我们今天主要讲一下懒汉模式的线程安全问题  ← 成功识别！
```

---

## 经验教训

1. **分层诊断的重要性**：不能只看最终结果，必须验证每个环节的中间数据
2. **文档与实现的差异**：模型文档说"输出 logits"，实际量化模型已包含 softmax
3. **训练条件的匹配**：CMVN 参数基于 int16 训练，推理时必须匹配幅度范围
4. **流式处理的状态管理**：跨块数据依赖需要显式缓冲和时间对齐

---

## 相关配置

### VadConfig（`src/subtitle/ivadengine.h`）
```cpp
struct VadConfig {
    std::string model_path;
    float threshold = 0.5f;           // 进入语音阈值
    float threshold_exit = 0.3f;      // 退出语音阈值（迟滞）
    int min_silence_ms = 300;         // 最短静音间隔（断句）
    int min_speech_ms = 250;          // 最短语音段长度
    int max_speech_ms = 20000;        // 单段最长语音（强制切分）
    int smoothing_window = 3;         // 概率滑动平均窗口
    int sample_rate = 16000;
};
```

### FSMN-VAD 模型配置（`config.yaml`）
```yaml
frontend_conf:
    fs: 16000
    window: hamming
    n_mels: 80
    frame_length: 25
    frame_shift: 10
    lfr_m: 5
    lfr_n: 1
    fft_size: 512
```

---

## Git 提交记录

- **aac37cf**: 修复 VAD 跨块截取和 PDF 解析（初版）
- **be0549c**: 固定 FBank FFT 大小为 512
- **[本次]**: 修复模型输出后验概率解析错误

---

## 参考资料

- [FunASR FSMN-VAD 模型](https://github.com/alibaba-damo-academy/FunASR)
- [Kaldi FBank 特征提取](https://kaldi-asr.org/doc/feat.html)
- ONNX Runtime C++ API 文档

---

**文档版本**: v1.0  
**创建时间**: 2026-08-08  
**最后更新**: 2026-08-08  
**作者**: Smart Player 开发团队
