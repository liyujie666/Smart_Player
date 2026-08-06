#ifndef IASRENGINE_H
#define IASRENGINE_H

#include <vector>
#include <string>
#include <memory>
#include "queue/subtitlequeue.h"

// ASR 引擎通用配置
struct AsrEngineConfig {
    std::string model_path;
    std::string language = "auto";   // "zh", "en", "ja", "auto"
    int n_threads = 4;
};

// ASR 引擎抽象接口
class IAsrEngine {
public:
    virtual ~IAsrEngine() = default;

    virtual bool init(const AsrEngineConfig& cfg) = 0;
    virtual void release() = 0;
    virtual bool isReady() const = 0;

    // 核心识别：输入 16kHz mono float32 PCM，输出带时间戳的字幕段
    virtual bool recognize(const std::vector<float>& pcm,
                           std::vector<SubtitleItem>& out,
                           double base_sec = 0.0) = 0;
    virtual void reset() = 0;

    // 引擎名称，用于日志和UI显示
    virtual std::string name() const = 0;
};

// 引擎类型枚举
enum class AsrEngineType {
    Whisper,
    SenseVoice,
    CloudASR
};

// 工厂函数
std::unique_ptr<IAsrEngine> createAsrEngine(AsrEngineType type);

#endif // IASRENGINE_H
