#ifndef ASROFFLINESTRATEGY_H
#define ASROFFLINESTRATEGY_H

#include "iasrstrategy.h"
#include "iasrengine.h"
#include "ivadengine.h"
#include "itranslator.h"
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

    // ASR 引擎类型
    void setAsrEngineType(AsrEngineType type) { engine_type_ = type; }

    // VAD 配置
    void setVadEnabled(bool enabled) { vad_enabled_ = enabled; }
    void setVadModelPath(const QString& path) { vad_model_path_ = path; }

    // 翻译配置
    void setTranslatorType(TranslatorType type) { translator_type_ = type; }
    void setTranslateConfig(const TranslateConfig& cfg) { translate_config_ = cfg; }
    void setTranslationEnabled(bool enabled) { translation_enabled_ = enabled; }

private:
    void run();
    void processAudioChunk(const std::vector<float>& pcm, double base_sec);

private:
    QString model_path_, url_;
    SubtitleQueue* queue_ = nullptr;
    std::unique_ptr<IAsrEngine> engine_;
    AsrEngineType engine_type_ = AsrEngineType::Whisper;

    // VAD
    bool vad_enabled_ = false;
    QString vad_model_path_;
    std::unique_ptr<IVadEngine> vad_;

    // 翻译
    TranslatorType translator_type_ = TranslatorType::GPT;
    TranslateConfig translate_config_;
    bool translation_enabled_ = false;
    std::unique_ptr<ITranslator> translator_;

    std::unique_ptr<Demuxer> demux_;
    std::unique_ptr<Decoder> dec_;
    std::unique_ptr<Resampler> res_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    bool uses_cached_model_ = false;
};

#endif
