#include "asrmanager.h"
#include "fileaudiosource.h"
#include "liveaudiosource.h"
#include "asrmodelcache.h"
#include <QDebug>

AsrManager::AsrManager(QObject *parent) : QObject(parent) {}
AsrManager::~AsrManager() { stop(); }

void AsrManager::setModelPath(const QString& path) {
    if (model_path_ == path) return;
    model_path_ = path;
    // AsrModelCache 仅服务于 Whisper 引擎
    if (asr_engine_type_ == AsrEngineType::Whisper) {
        AsrModelCache::instance().setModelPath(path);
    }
}

void AsrManager::warmUp() {
    if (model_path_.isEmpty()) return;
    // AsrModelCache 仅服务于 Whisper 引擎
    if (asr_engine_type_ == AsrEngineType::Whisper) {
        AsrModelCache::instance().setModelPath(model_path_);
    }
}

bool AsrManager::init(const QString& url, Demuxer::MediaType type, AVStream* audio) {
    stop();
    last_type_ = type;

    // 1. 创建音频源
    std::unique_ptr<IAudioSource> source;
    if (type == Demuxer::MediaType::RTSP_TYPE || type == Demuxer::MediaType::RTMP_TYPE) {
        // 实时模式：LiveAudioSource
        auto live = std::make_unique<LiveAudioSource>(audio);
        if (!live->open()) {
            qWarning() << "[AsrManager] LiveAudioSource open failed";
            return false;
        }
        source = std::move(live);
        queue_.setMode(SubtitleQueue::Mode::Live);
    } else {
        // 离线模式：FileAudioSource
        auto file = std::make_unique<FileAudioSource>(url);
        if (!file->open()) {
            qWarning() << "[AsrManager] FileAudioSource open failed";
            return false;
        }
        source = std::move(file);
        queue_.setMode(SubtitleQueue::Mode::Offline);
    }

    // 2. 创建并初始化 Pipeline
    pipeline_ = std::make_unique<AsrPipeline>(this);
    pipeline_->setSource(std::move(source));

    // 离线识别节流：把播放位置提供者传给 Pipeline
    if (playback_pos_fn_) {
        pipeline_->setPlaybackPositionProvider(playback_pos_fn_);
    }

    // 连接信号（Pipeline 的 emit 发生在 std::thread 中，必须用 QueuedConnection）
    connect(pipeline_.get(), &AsrPipeline::subtitleReady,
            this, &AsrManager::subtitleReady, Qt::QueuedConnection);
    connect(pipeline_.get(), &AsrPipeline::translationReady,
            this, &AsrManager::translationReady, Qt::QueuedConnection);
    connect(pipeline_.get(), &AsrPipeline::engineError,
            this, &AsrManager::engineError, Qt::QueuedConnection);

    // 3. 用配置初始化管线（VAD + ASR + 翻译引擎）
    auto cfg = buildPipelineConfig();
    if (!pipeline_->init(cfg, audio, &queue_)) {
        qWarning() << "[AsrManager] Pipeline init failed";
        pipeline_.reset();
        return false;
    }

    return true;
}

void AsrManager::initAsync(const QString& url, Demuxer::MediaType type, AVStream* audio) {
    // 等待之前的异步初始化完成
    if (init_thread_.joinable()) {
        init_cancelling_ = true;
        init_thread_.join();
        init_cancelling_ = false;
    }

    // 复制参数（audio 流指针在调用期间有效，确保播放器不先析构）
    QString url_copy = url;
    Demuxer::MediaType type_copy = type;
    AVStream* audio_copy = audio;

    init_thread_ = std::thread([this, url_copy, type_copy, audio_copy]() {
        qDebug() << "[AsrManager] async init started";

        // 先停止旧 pipeline（在工作线程做，避免主线程阻塞）
        {
            std::lock_guard<std::mutex> lock(init_mtx_);
            if (pipeline_) {
                pipeline_->stop();
                pipeline_.reset();
            }
            queue_.clear();
        }

        if (init_cancelling_) return;

        // 同步执行 init
        bool ok = false;
        {
            std::lock_guard<std::mutex> lock(init_mtx_);
            ok = init(url_copy, type_copy, audio_copy);
        }

        if (init_cancelling_) return;

        if (ok) {
            qDebug() << "[AsrManager] async init success, starting pipeline";
            std::lock_guard<std::mutex> lock(init_mtx_);
            if (pipeline_ && !init_cancelling_) {
                pipeline_->start();
            }
        } else {
            qWarning() << "[AsrManager] async init failed";
            emit engineError("ASR init failed");
        }
    });
}

void AsrManager::start() {
    if (pipeline_) pipeline_->start();
}

void AsrManager::stop() {
    // 取消异步初始化（如果在进行中）
    init_cancelling_ = true;
    if (init_thread_.joinable()) {
        init_thread_.join();
    }
    init_cancelling_ = false;

    std::lock_guard<std::mutex> lock(init_mtx_);
    if (pipeline_) {
        pipeline_->stop();
        pipeline_.reset();
    }
    queue_.clear();
}

void AsrManager::reset() {
    if (pipeline_) pipeline_->reset();
}

void AsrManager::sendAudioFrame(AVFrame* frame) {
    if (pipeline_) pipeline_->feedAudio(frame);
}

// ===== 配置接口实现 =====

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
