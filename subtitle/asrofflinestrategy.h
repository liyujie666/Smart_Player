#ifndef ASROFFLINESTRATEGY_H
#define ASROFFLINESTRATEGY_H

#include "iasrstrategy.h"
#include "asrworker.h"
#include "demuxer/demuxer.h"
#include "decoder/decoder.h"
#include "resampler/resampler.h"
#include <thread>

class AsrOfflineStrategy : public IAsrStrategy {
public:
    AsrOfflineStrategy();
    ~AsrOfflineStrategy() override;

    bool init(const QString& url, AVStream*, SubtitleQueue* queue) override;
    void start() override;
    void stop() override;
    void reset() override;
    void release() override;
    void setModel(const QString& path) { model_path_ = path; }

private:
    void run();

private:
    QString model_path_, url_;
    SubtitleQueue* queue_ = nullptr;
    std::unique_ptr<AsrWorker> worker_;
    std::unique_ptr<Demuxer> demux_;
    std::unique_ptr<Decoder> dec_;
    std::unique_ptr<Resampler> res_;
    std::thread thread_;
    bool running_ = false;
};

#endif
