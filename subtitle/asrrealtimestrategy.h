#ifndef ASRREALTIMESTRATEGY_H
#define ASRREALTIMESTRATEGY_H

#include "iasrstrategy.h"
#include "resampler/resampler.h"
#include "utils/audioringbuffer.h"
#include <thread>

class AsrWorker;
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
    void setModel(const QString& path) { model_path_ = path; }

private:
    void run();

private:
    QString model_path_;
    SubtitleQueue* queue_ = nullptr;
    std::unique_ptr<AsrWorker> worker_;
    std::unique_ptr<Resampler> resampler_;
    AudioPcmRingBuffer ring_;
    std::thread thread_;
    bool running_ = false;
    AVRational tb_{0,0};
    std::string last_text_;
};

#endif // ASRREALTIMESTRATEGY_H
