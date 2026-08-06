#ifndef FSMNVAD_H
#define FSMNVAD_H

#include "ivadengine.h"
#include <vector>
#include <string>

// FSMN-VAD 实现
// 使用 FunASR 的 FSMN-VADONNX 模型进行语音活动检测
// 模型推理基于 onnxruntime
struct OrtSession;
struct OrtEnv;

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
    // 内部帧级推理
    float inferFrame(const std::vector<float>& frame_data);

    // 状态机：帧级决策 → 段级输出
    void updateState(float speech_prob, double frame_time);

private:
    VadConfig cfg_;
    bool ready_ = false;

    //ONNX Runtime 会话（前向声明，实现中包含完整头文件）
    struct OrtContext;
    std::unique_ptr<OrtContext> ort_ctx_;

    // FSMN 内部状态缓存（隐藏层记忆）
    std::vector<float> cache_;

    // VAD 状态机
    enum class State { Silence, Speech, Trailing };
    State state_ = State::Silence;

    // 当前语音段的起止
    double current_speech_start_ = 0.0;
    double current_speech_end_ = 0.0;

    // 静音计数器（帧为单位）
    int silence_frame_count_ = 0;

    // 帧参数
    int frame_size_ = 400;    // 25ms @ 16kHz
    int frame_shift_ = 160;   // 10ms @ 16kHz

    // 累积的完整语音段（process返回后清空）
    std::vector<VadSegment> completed_segments_;

    // 流式输入残余
    std::vector<float> residual_pcm_;
    double residual_base_sec_ = 0.0;
    int64_t total_frames_processed_ = 0;
};

#endif // FSMNVAD_H
