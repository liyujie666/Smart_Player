#ifndef ASROFFLINESTRATEGY_H
#define ASROFFLINESTRATEGY_H

#include "iasrstrategy.h"
#include "iasrengine.h"
#include "asrmodelcache.h"
#include "demuxer/demuxer.h"
#include "decoder/decoder.h"
#include "resampler/resampler.h"
#include <thread>
#include <atomic>

class AsrOfflineStrategy : public IAsrStrategy {
public:
    AsrOfflineStrategy();
    ~AsrOfflineStrategy() override;

    bool init(const QString& url, AVStream*, SubtitleQueue* queue) override;
    void start() override;
    void stop() override;
    void reset() override;
    void release() override;
    void setModel(const QString& path) override { model_path_ = path; }

    // 支持动态切换 ASR 引擎类型
    void setAsrEngineType(AsrEngineType type) { engine_type_ = type; }

private:
    void run();

private:
    QString model_path_, url_;
    SubtitleQueue* queue_ = nullptr;
    std::unique_ptr<IAsrEngine> engine_;
    AsrEngineType engine_type_ = AsrEngineType::Whisper;
    std::unique_ptr<Demuxer> demux_;
    std::unique_ptr<Decoder> dec_;
    std::unique_ptr<Resampler> res_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    bool uses_cached_model_ = false;
};

#endif
