#ifndef ASRMANAGER_H
#define ASRMANAGER_H

#include <QObject>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
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
    // 异步初始化+启动：模型加载在工作线程完成，不阻塞调用线程
    void initAsync(const QString& url, Demuxer::MediaType type, AVStream* audio);
    bool isInitializing() const { return init_thread_.joinable(); }
    void start();
    void stop();
    void releaseEngines();   // 释放缓存的引擎（程序退出时调用）
    void reset();
    void sendAudioFrame(AVFrame* frame);
    SubtitleQueue* queue() { return &queue_; }

    void setModelPath(const QString& path);
    bool isModelPathEmpty() const { return model_path_.isEmpty(); }

    // 离线识别节流：提供当前播放位置（秒），避免识别过度超前占满 CPU
    void setPlaybackPositionProvider(std::function<double()> fn) {
        playback_pos_fn_ = std::move(fn);
    }

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
    bool initInternal(const QString& url, Demuxer::MediaType type, AVStream* audio);
    PipelineConfig buildPipelineConfig() const;

private:
    QString model_path_;
    SubtitleQueue queue_;
    Demuxer::MediaType last_type_ = Demuxer::MediaType::FILE_TYPE;

    // 新架构：Pipeline + Source
    std::unique_ptr<AsrPipeline> pipeline_;

    // 引擎缓存（跨文件复用，不随 Pipeline 重建）
    std::unique_ptr<IVadEngine> cached_vad_;
    std::unique_ptr<IAsrEngine> cached_asr_;
    std::unique_ptr<ITranslator> cached_translator_;
    AsrEngineType cached_asr_type_ = AsrEngineType::Whisper;
    QString cached_vad_model_path_;
    QString cached_asr_model_path_;
    TranslatorType cached_translator_type_ = TranslatorType::GPT;

    // 异步初始化线程（防止 ONNX 模型加载阻塞调用线程）
    std::thread init_thread_;
    std::mutex init_mtx_;               // 保护 pipeline_ 的创建/销毁
    std::atomic<bool> init_cancelling_{false};

    // 引擎配置
    AsrEngineType asr_engine_type_ = AsrEngineType::Whisper;
    bool vad_enabled_ = true;
    QString vad_model_path_;
    TranslatorType translator_type_ = TranslatorType::GPT;
    TranslateConfig translate_config_;
    bool translation_enabled_ = false;

    // 离线识别节流：播放位置提供者
    std::function<double()> playback_pos_fn_;
};

#endif
