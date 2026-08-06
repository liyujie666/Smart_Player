#ifndef ASRWORKER_H
#define ASRWORKER_H

//============================================================
// 已废弃：AsrWorker 已重构为 WhisperEngine (实现 IAsrEngine 接口)
// 此文件保留作为兼容层，将AsrWorker 类型别名指向 WhisperEngine
// 新代码请直接使用 IAsrEngine / WhisperEngine
// ============================================================

#include "whisperengine.h"
#include "queue/subtitlequeue.h"

// 兼容旧配置结构
struct AsrConfig {
    std::string model_path;
    std::string language = "auto";
    bool translate = false;
};

// AsrWorker 现在是 WhisperEngine 的轻量包装，保持向后兼容
class AsrWorker {
    AsrWorker(const AsrWorker&) = delete;
    AsrWorker& operator=(const AsrWorker&) = delete;
public:
    AsrWorker() : engine_(std::make_unique<WhisperEngine>()) {}
    ~AsrWorker() = default;

    bool initWithContext(whisper_context* external_ctx, const AsrConfig& cfg) {
        AsrEngineConfig ecfg;
        ecfg.model_path = cfg.model_path;
        ecfg.language = cfg.language;
        return engine_->initWithContext(external_ctx, ecfg);
    }

    bool init(const AsrConfig& cfg) {
        AsrEngineConfig ecfg;
        ecfg.model_path = cfg.model_path;
        ecfg.language = cfg.language;
        return engine_->init(ecfg);
    }

    void release() { engine_->release(); }
    bool isReady() const { return engine_->isReady(); }

    bool recognize(const std::vector<float>& pcm, std::vector<SubtitleItem>& out, double base_sec = 0) {
        return engine_->recognize(pcm, out, base_sec);
    }

    void reset() { engine_->reset(); }

private:
    std::unique_ptr<WhisperEngine> engine_;
};

#endif // ASRWORKER_H
