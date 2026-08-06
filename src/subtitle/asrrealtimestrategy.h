#ifndef ASRREALTIMESTRATEGY_H
#define ASRREALTIMESTRATEGY_H

#include "iasrstrategy.h"
#include "iasrengine.h"
#include "resampler/resampler.h"
#include "utils/audioringbuffer.h"
#include <thread>
#include <atomic>

class AsrRealtimeStrategy : public IAsrStrategy {
public:
    AsrRealtimeStrategy();
    ~AsrRealtimeStrategy() override;

    bool init(const QString&, AVStream* audio, SubtitleQueue* queue) override;
    void start() override;
    void stop() override;
    void reset() override;
    void sendAudio(AVFrame* frame) override;
    void release() override;
    void setModel(const QString& path) override { model_path_ = path; }

    // 支持动态切换 ASR 引擎类型
    void setAsrEngineType(AsrEngineType type) { engine_type_ = type; }

private:
    void run();

private:
    QString model_path_;
    SubtitleQueue* queue_ = nullptr;
    std::unique_ptr<IAsrEngine> engine_;
    AsrEngineType engine_type_ = AsrEngineType::Whisper;
    std::unique_ptr<Resampler> resampler_;
    AudioPcmRingBuffer ring_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    AVRational tb_{0,0};
    std::string last_text_;
    bool uses_cached_model_ = false;
};

#endif // ASRREALTIMESTRATEGY_H
