#ifndef ASRWORKER_H
#define ASRWORKER_H

#include <vector>
#include <string>
#include <memory>
#include "whisper/whisper.h"
#include "queue/subtitlequeue.h"

struct AsrConfig{
    std::string model_path;
    std::string language = "auto";
    bool translate = false;
};

class AsrWorker{
    AsrWorker(const AsrWorker&) = delete;
    AsrWorker& operator=(const AsrWorker&) = delete;
    AsrWorker(AsrWorker&&) = delete;
    AsrWorker& operator=(AsrWorker&&) = delete;
public:
    AsrWorker() = default;
    ~AsrWorker();

    // 优先使用外部传入的已加载上下文（由 AsrModelCache 提供）
    // 返回 true 表示使用外部上下文成功
    bool initWithContext(whisper_context* external_ctx, const AsrConfig& cfg);

    // 备用方案：自己加载模型文件（当缓存未命中时使用）
    bool init(const AsrConfig& cfg);

    void release();
    bool isReady() const { return ctx_ != nullptr; }

    bool recognize(const std::vector<float>& pcm, std::vector<SubtitleItem>& out, double base_sec = 0);
    void reset();

private:
    whisper_context* ctx_ = nullptr;
    AsrConfig cfg_;
    bool owns_context_ = false;
};

#endif // ASRWORKER_H
