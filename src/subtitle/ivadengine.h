#ifndef IVADENGINE_H
#define IVADENGINE_H

#include <vector>
#include <string>
#include <memory>

// 语音活动区间
struct VadSegment {
    double start_sec = 0.0;
    double end_sec = 0.0;
};

struct VadConfig {
    std::string model_path;
    float threshold = 0.5f;           // 进入语音阈值（高阈值，避免误触发）
    float threshold_exit = 0.3f;      // 退出语音阈值（低阈值，迟滞防振荡）
    int min_silence_ms = 300;         // 最短静音间隔（用于断句）
    int min_speech_ms = 250;          // 最短语音段长度
    int smoothing_window = 3;         // 概率滑动平均窗口（帧数，0=关闭）
    int sample_rate = 16000;
};

class IVadEngine {
public:
    virtual ~IVadEngine() = default;

    virtual bool init(const VadConfig& cfg) = 0;
    virtual void release() = 0;
    virtual bool isReady() const = 0;

    // 流式接口：喂入音频块，返回检测到的完整语音段
    // pcm: 16kHz mono float32
    virtual std::vector<VadSegment> process(const std::vector<float>& pcm,
                                            double base_sec = 0.0) = 0;

    // 强制刷新（流结束时获取残余的最后一段）
    virtual std::vector<VadSegment> flush() = 0;

    virtual void reset() = 0;
    virtual std::string name() const = 0;
};

// VAD 引擎类型
enum class VadEngineType {
    FSMN,// FunASR FSMN-VAD（推荐）
    Silero      // Silero VAD（备选）
};

// 工厂函数
std::unique_ptr<IVadEngine> createVadEngine(VadEngineType type);

#endif // IVADENGINE_H
