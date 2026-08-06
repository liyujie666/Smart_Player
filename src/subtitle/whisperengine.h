#ifndef WHISPERENGINE_H
#define WHISPERENGINE_H

#include "iasrengine.h"
#include "whisper/whisper.h"

class AsrModelCache;

class WhisperEngine : public IAsrEngine {
    WhisperEngine(const WhisperEngine&) = delete;
    WhisperEngine& operator=(const WhisperEngine&) = delete;
public:
    WhisperEngine();
    ~WhisperEngine() override;

    bool init(const AsrEngineConfig& cfg) override;
    void release() override;
    bool isReady() const override { return ctx_ != nullptr; }

    bool recognize(const std::vector<float>& pcm,
                   std::vector<SubtitleItem>& out,
                   double base_sec = 0.0) override;
    void reset() override;

    std::string name() const override { return "Whisper"; }

    // Whisper 特有：支持使用外部缓存的context
    bool initWithContext(whisper_context* external_ctx, const AsrEngineConfig& cfg);

    // 是否正在使用缓存的 context（供外部管理生命周期）
    bool usesCachedContext() const { return !owns_context_; }

private:
    whisper_context* ctx_ = nullptr;
    AsrEngineConfig cfg_;
    bool owns_context_ = false;
};

#endif // WHISPERENGINE_H
