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
        strategy_ = std::move(s);
        queue_.setMode(SubtitleQueue::Mode::Live);
    } else {
        auto s = std::make_unique<AsrOfflineStrategy>();
        s->setModel(model_path_);
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
