#include "asrmanager.h"
#include "asrrealtimestrategy.h"
#include "asrofflinestrategy.h"
#include "asrmodelcache.h"

AsrManager::AsrManager(QObject *parent) : QObject(parent) {}
AsrManager::~AsrManager() { stop(); }

void AsrManager::setModelPath(const QString& path) {
    if (model_path_ == path) return;
    model_path_ = path;
    AsrModelCache::instance().setModelPath(path);
}

void AsrManager::warmUp() {
    if (model_path_.isEmpty()) return;
    AsrModelCache::instance().setModelPath(model_path_);
}

void AsrManager::switchMode(Demuxer::MediaType type) {
    if (strategy_ && last_type_ == type) return;
    stop();

    queue_.clear();
    strategy_.reset();
    last_type_ = type;

    if (type == Demuxer::MediaType::RTSP_TYPE || type == Demuxer::MediaType::RTMP_TYPE) {
        auto s = std::make_unique<AsrRealtimeStrategy>();
        s->setModel(model_path_);
        s->setAsrEngineType(asr_engine_type_);
        s->setVadEnabled(vad_enabled_);
        s->setVadModelPath(vad_model_path_);
        s->setTranslatorType(translator_type_);
        s->setTranslateConfig(translate_config_);
        s->setTranslationEnabled(translation_enabled_);
        strategy_ = std::move(s);
        queue_.setMode(SubtitleQueue::Mode::Live);
    } else {
        auto s = std::make_unique<AsrOfflineStrategy>();
        s->setModel(model_path_);
        s->setAsrEngineType(asr_engine_type_);
        s->setVadEnabled(vad_enabled_);
        s->setVadModelPath(vad_model_path_);
        s->setTranslatorType(translator_type_);
        s->setTranslateConfig(translate_config_);
        s->setTranslationEnabled(translation_enabled_);
        strategy_ = std::move(s);
        queue_.setMode(SubtitleQueue::Mode::Offline);
    }
}

bool AsrManager::init(const QString& url, Demuxer::MediaType type, AVStream* audio) {
    stop();
    switchMode(type);
    return strategy_->init(url, audio, &queue_);
}

void AsrManager::start() {
    if (strategy_) strategy_->start();
}

void AsrManager::stop() {
    if (strategy_) strategy_->stop();
    queue_.clear();
}

void AsrManager::reset() {
    if (strategy_) strategy_->reset();
}

void AsrManager::sendAudioFrame(AVFrame* frame) {
    if (strategy_) strategy_->sendAudio(frame);
}

// ===== 新增接口实现 =====

void AsrManager::setAsrEngineType(AsrEngineType type) {
    asr_engine_type_ = type;
}

void AsrManager::setVadEnabled(bool enabled) {
    vad_enabled_ = enabled;
}

void AsrManager::setTranslatorType(TranslatorType type) {
    translator_type_ = type;
}

void AsrManager::setTranslationEnabled(bool enabled) {
    translation_enabled_ = enabled;
}

void AsrManager::setPipelineConfig(const PipelineConfig& cfg) {
    asr_engine_type_ = cfg.asr_type;
    vad_enabled_ = cfg.enable_vad;
    translation_enabled_ = cfg.enable_translation;
    translator_type_ = cfg.translator_type;
    translate_config_ = cfg.translate_config;
}

PipelineConfig AsrManager::buildPipelineConfig() const {
    PipelineConfig cfg;
    cfg.enable_vad = vad_enabled_;
    cfg.vad_type = VadEngineType::FSMN;
    cfg.vad_config.model_path = vad_model_path_.toStdString();

    cfg.asr_type = asr_engine_type_;
    cfg.asr_config.model_path = model_path_.toStdString();

    cfg.enable_translation = translation_enabled_;
    cfg.translator_type = translator_type_;
    cfg.translate_config = translate_config_;

    return cfg;
}
