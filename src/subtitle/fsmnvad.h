#ifndef FSMNVAD_H
#define FSMNVAD_H

#include "ivadengine.h"
#include <vector>
#include <string>
#include <memory>

#if HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

// FSMN-VAD 实现
// 使用 FunASR 的 FSMN-VAD ONNX 模型进行语音活动检测
// 模型输入: speech (1, T) + cache
// 模型输出: logits (1, T_frames, 2) + cache_out
class FsmnVad : public IVadEngine {
public:
    FsmnVad();
    ~FsmnVad() override;

    bool init(const VadConfig& cfg) override;
    void release() override;
    bool isReady() const override { return ready_; }

    std::vector<VadSegment> process(const std::vector<float>& pcm,
                                    double base_sec = 0.0) override;
    std::vector<VadSegment> flush() override;

    void reset() override;
    std::string name() const override { return "FSMN-VAD"; }

private:
    float inferChunk(const std::vector<float>& chunk);
    void updateState(float speech_prob, double frame_time);

    // 加载模型目录（model.onnx + am.mvn + config.yaml）
    bool loadModel(const std::string& model_dir);

private:
    VadConfig cfg_;
    bool ready_ = false;

#if HAS_ONNXRUNTIME
    struct OrtContext {
        std::unique_ptr<Ort::Session> session;
        std::unique_ptr<Ort::SessionOptions> session_options;

        std::vector<const char*> input_names;
        std::vector<const char*> output_names;
        std::vector<Ort::AllocatedStringPtr> input_name_ptrs;
        std::vector<Ort::AllocatedStringPtr> output_name_ptrs;

        Ort::MemoryInfo memory_info{nullptr};
        Ort::Value input_tensor{nullptr};
        Ort::Value cache_tensor{nullptr};

        // 模型输入输出信息
        int64_t cache_dim = 0;        // 缓存维度
        int64_t chunk_size = 0;       // 模型期望的输入块大小
        int64_t cache_shape_0 = 0;    // cache shape[0]
    };
    std::unique_ptr<OrtContext> ort_ctx_;
    std::vector<float> cache_;       // FSMN 隐藏状态缓存
#endif

    // CMVN 参数
    std::vector<float> cmvn_mean_;
    std::vector<float> cmvn_var_;

    // VAD 状态机
    enum class State { Silence, Speech, Trailing };
    State state_ = State::Silence;

    double current_speech_start_ = 0.0;
    double current_speech_end_ = 0.0;
    int silence_frame_count_ = 0;

    // 帧参数（FSMN-VAD 通常用 10ms 粒度的输出）
    int chunk_samples_ = 2560;    // 160ms @ 16kHz (模型期望的输入块)
    int frame_shift_ms_ = 10;    // 输出帧间隔

    std::vector<VadSegment> completed_segments_;

    // 流式输入残余
    std::vector<float> residual_pcm_;
    double residual_base_sec_ = 0.0;
    int64_t total_frames_processed_ = 0;
};

#endif // FSMNVAD_H
