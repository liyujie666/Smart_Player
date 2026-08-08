#ifndef ASRMANAGER_H
#define ASRMANAGER_H

#include <QObject>
#include <memory>
#include "queue/subtitlequeue.h"
#include "iasrengine.h"
#include "itranslator.h"
#include "ivadengine.h"
#include "iaudiosource.h"
#include "asrpipeline.h"
#include "demuxer/demuxer.h"

class AsrManager : public QObject {
    Q_OBJECT
public:
    explicit AsrManager(QObject *parent = nullptr);
    ~AsrManager() override;

    bool init(const QString& url, Demuxer::MediaType type, AVStream* audio);
    void start();
    void stop();
    void reset();
    void sendAudioFrame(AVFrame* frame);
    SubtitleQueue* queue() { return &queue_; }

    void setModelPath(const QString& path);
    bool isModelPathEmpty() const { return model_path_.isEmpty(); }

    // 预加载模型
    void warmUp();

    // ===== 多引擎管理接口 =====

    // ASR 引擎切换
    void setAsrEngineType(AsrEngineType type);
    AsrEngineType asrEngineType() const { return asr_engine_type_; }

// VAD 开关
    void setVadEnabled(bool enabled);
    bool isVadEnabled() const { return vad_enabled_; }
    void setVadModelPath(const QString& path) { vad_model_path_ = path; }

    // 翻译引擎配置
    void setTranslatorType(TranslatorType type);
    TranslatorType translatorType() const { return translator_type_; }
    void setTranslateConfig(const TranslateConfig& cfg) { translate_config_ = cfg; }
  void setTranslationEnabled(bool enabled);
    bool isTranslationEnabled() const { return translation_enabled_; }

    // 管线配置（一次性设置完整配置）
    void setPipelineConfig(const PipelineConfig& cfg);

signals:
void subtitleReady(const SubtitleItem& item);
    void translationReady(const SubtitleItem& item);
  void engineError(const QString& error);

private:
    PipelineConfig buildPipelineConfig() const;

private:
    QString model_path_;
    SubtitleQueue queue_;
    Demuxer::MediaType last_type_ = Demuxer::MediaType::FILE_TYPE;

    // 新架构：Pipeline + Source
    std::unique_ptr<AsrPipeline> pipeline_;

    // 引擎配置
    AsrEngineType asr_engine_type_ = AsrEngineType::Whisper;
    bool vad_enabled_ = true;
    QString vad_model_path_;
    TranslatorType translator_type_ = TranslatorType::GPT;
    TranslateConfig translate_config_;
    bool translation_enabled_ = false;
};

#endif
