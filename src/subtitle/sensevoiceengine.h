#ifndef SENSEVOICEENGINE_H
#define SENSEVOICEENGINE_H

#include "iasrengine.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#if HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

// SenseVoice ASR 引擎
// 基于 FunASR SenseVoice 模型，使用 ONNX Runtime 本地推理
// 模型输入: speech (1, T, n_mels), speech_lengths (1)
// 模型输出: ctc_logits (1, T, vocab_size), encoder_out_lens (1)
class SenseVoiceEngine : public IAsrEngine {
public:
    SenseVoiceEngine();
    ~SenseVoiceEngine() override;

    bool init(const AsrEngineConfig& cfg) override;
    void release() override;
    bool isReady() const override { return ready_; }

    bool recognize(const std::vector<float>& pcm,
                   std::vector<SubtitleItem>& out,
                   double base_sec = 0.0) override;
    void reset() override;

    std::string name() const override { return "SenseVoice"; }

private:
    // 加载模型目录
    bool loadModel(const std::string& model_dir);

    // 加载 tokens.json
    bool loadTokens(const std::string& tokens_path);

    // 读取 config.yaml 获取模型参数
    bool loadConfig(const std::string& config_path);

    // CTC greedy 解码
    std::string ctcDecode(const int64_t* token_ids, int length);

    // 后处理：去除 <|zh|> <|HAPPY|> <|Speech|> 等标签
    std::string postProcess(const std::string& text);

#if HAS_ONNXRUNTIME
    struct Impl {
        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<const char*> input_names;
        std::vector<const char*> output_names;
        std::vector<Ort::AllocatedStringPtr> input_name_ptrs;
        std::vector<Ort::AllocatedStringPtr> output_name_ptrs;

        int n_mels = 80;       // 模型输入维度（含 LFR 拼接）
        int vocab_size = 0;
        int lfr_m = 7;         // LFR 拼接帧数（model_feat_dim / 80）

        // 输入索引（SenseVoice 4 输入），-1 表示该输入不存在
        int idx_speech = 0;
        int idx_lengths = 1;
        int idx_language = -1;
        int idx_textnorm = -1;
    };
    std::unique_ptr<Impl> impl_;
#endif

    // token 表: token_id → text
    std::unordered_map<int, std::string> token_table_;

    // 模型参数
    int n_mels_ = 80;
    int frame_length_ = 400;   // 25ms
    int frame_shift_ = 160;    // 10ms

    AsrEngineConfig cfg_;
    bool ready_ = false;
};

#endif // SENSEVOICEENGINE_H
