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
// 模型输入: speech (1, T, 400) =80维FBank经LFR(m=5,n=1)拼接+CMVN, in_cache0~3
// 模型输出: logits (1, T, 2) + out_cache0~3
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
    // 对一批特征帧做推理，返回每帧的 speech 概率；失败返回空
    std::vector<float> inferFeatures(const std::vector<float>& flat_feats, int num_frames);
    void updateState(float speech_prob, double frame_time);

    // PCM → FBank(80) → LFR(m=5,n=1)→400维 → CMVN
    std::vector<float> computeFeatures(const std::vector<float>& pcm, int& out_frames);

    // 加载模型目录（model.onnx + am.mvn + config.yaml）
    bool loadModel(const std::string& model_dir);

private:
    VadConfig cfg_;
    bool ready_ = false;

#if HAS_ONNXRUNTIME
    struct OrtContext {
        std::unique_ptr<Ort::Session> session;

        std::vector<const char*> input_names;
        std::vector<const char*> output_names;
        std::vector<Ort::AllocatedStringPtr> input_name_ptrs;
        std::vector<Ort::AllocatedStringPtr> output_name_ptrs;

        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        int feat_dim = 400;                                // speech 输入最后一维
        std::vector<std::vector<int64_t>> cache_shapes;    // 各 cache 输入的 shape
    };
    std::unique_ptr<OrtContext> ort_ctx_;
    std::vector<std::vector<float>> caches_;   // FSMN 各隐藏状态缓存
#endif

    // CMVN 参数（400 维，与 LFR 后特征维度一致）
    std::vector<float> cmvn_mean_;
    std::vector<float> cmvn_var_;

    // VAD 状态机
    enum class State { Silence, Speech, Trailing };
    State state_ = State::Silence;

    double current_speech_start_ = 0.0;
    double current_speech_end_ = 0.0;
    int silence_frame_count_ = 0;

    // 概率滑动平均
    std::vector<float> prob_history_;

    // FBank / LFR 参数
    int n_mels_ = 80;
    int lfr_m_ = 5;               // LFR 拼接帧数
    int lfr_n_ = 1;               // LFR 下采样率
    int frame_shift_ms_ = 10;     // 输出帧间隔（LFR n=1 时等于 fbank 步进）
    int infer_chunk_frames_ = 64; // 每次推理的特征帧数

    std::vector<VadSegment> completed_segments_;

    // 流式输入残余（PCM 与已提取但未消费的特征）
    std::vector<float> residual_pcm_;
    double stream_base_sec_ = 0.0;
    bool base_initialized_ = false;
    bool diagnostic_logged_ = false;
    int64_t total_frames_processed_ = 0;
};

#endif // FSMNVAD_H
