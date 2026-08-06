#ifndef SENSEVOICEENGINE_H
#define SENSEVOICEENGINE_H

#include "iasrengine.h"
#include <vector>
#include <string>
#include <memory>

// SenseVoice ASR 引擎
// 基于 FunASR SenseVoice 模型，使用 ONNX Runtime 本地推理
// 特点：中英双语、低延迟、支持情感/事件检测标签
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
    struct Impl;
    std::unique_ptr<Impl> impl_;
    AsrEngineConfig cfg_;
    bool ready_ = false;
};

#endif // SENSEVOICEENGINE_H
