#ifndef ASRREALTIMESTRATEGY_H
#define ASRREALTIMESTRATEGY_H

#include "iasrstrategy.h"
#include "iasrengine.h"
#include "ivadengine.h"
#include "itranslator.h"
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

private:
    QString model_path_;
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

    std::unique_ptr<Resampler> resampler_;
    AudioPcmRingBuffer ring_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    AVRational tb_{0,0};
    std::string last_text_;
    bool uses_cached_model_ = false;
};

#endif // ASRREALTIMESTRATEGY_H
