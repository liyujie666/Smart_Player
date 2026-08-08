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
    std::lock_guard<std::mutex> lock(init_mtx_);
    return initInternal(url, type, audio);
}

bool AsrManager::initInternal(const QString& url, Demuxer::MediaType type, AVStream* audio) {
    last_type_ = type;

    // 1. 创建音频源
    std::unique_ptr<IAudioSource> source;
    if (type == Demuxer::MediaType::RTSP_TYPE || type == Demuxer::MediaType::RTMP_TYPE) {
        auto live = std::make_unique<LiveAudioSource>(audio);
        if (!live->open()) {
            qWarning() << "[AsrManager] LiveAudioSource open failed";
            return false;
        }
        source = std::move(live);
        queue_.setMode(SubtitleQueue::Mode::Live);
    } else {
        auto file = std::make_unique<FileAudioSource>(url);
        if (!file->open()) {
            qWarning() << "[AsrManager] FileAudioSource open failed";
            return false;
        }
        source = std::move(file);
        queue_.setMode(SubtitleQueue::Mode::Offline);
    }

    // 2. 确保引擎已加载（跨文件复用，仅在配置变化时重建）
    auto cfg = buildPipelineConfig();

    // VAD 引擎
    if (cfg.enable_vad) {
        bool need_rebuild = !cached_vad_ ||
            cached_vad_model_path_.toStdString() != cfg.vad_config.model_path;
        if (need_rebuild) {
            qDebug() << "[AsrManager] (re)loading VAD engine";
            cached_vad_ = createVadEngine(cfg.vad_type);
            if (cached_vad_ && !cached_vad_->init(cfg.vad_config)) {
                qDebug() << "[AsrManager] VAD init failed, disabling VAD";
                cached_vad_.reset();
                cfg.enable_vad = false;
            } else {
                cached_vad_model_path_ = QString::fromStdString(cfg.vad_config.model_path);
            }
        } else {
            qDebug() << "[AsrManager] reusing cached VAD engine";
            cached_vad_->reset();
        }
    }

    // ASR 引擎
    bool need_rebuild_asr = !cached_asr_ ||
        cached_asr_type_ != cfg.asr_type ||
        cached_asr_model_path_.toStdString() != cfg.asr_config.model_path;
    if (need_rebuild_asr) {
        qDebug() << "[AsrManager] (re)loading ASR engine:" << (int)cfg.asr_type;
        cached_asr_ = createAsrEngine(cfg.asr_type);
        if (!cached_asr_) {
            emit engineError("Failed to create ASR engine");
            return false;
        }
        if (!cached_asr_->init(cfg.asr_config)) {
            emit engineError("ASR engine init failed");
            cached_asr_.reset();
            return false;
        }
        cached_asr_type_ = cfg.asr_type;
        cached_asr_model_path_ = QString::fromStdString(cfg.asr_config.model_path);
    } else {
        qDebug() << "[AsrManager] reusing cached ASR engine";
        cached_asr_->reset();
    }

    // 翻译引擎
    if (cfg.enable_translation) {
        bool need_rebuild_tr = !cached_translator_ ||
            cached_translator_type_ != cfg.translator_type;
        if (need_rebuild_tr) {
            qDebug() << "[AsrManager] (re)loading translator:" << (int)cfg.translator_type;
            cached_translator_ = createTranslator(cfg.translator_type);
            if (cached_translator_ && !cached_translator_->init(cfg.translate_config)) {
                qDebug() << "[AsrManager] translator init failed, disabling translation";
                cached_translator_.reset();
                cfg.enable_translation = false;
            } else {
                cached_translator_type_ = cfg.translator_type;
            }
        } else {
            qDebug() << "[AsrManager] reusing cached translator";
        }
    }

    // 3. 创建 Pipeline 并注入引擎（不重新加载模型）
    pipeline_ = std::make_unique<AsrPipeline>();
    pipeline_->moveToThread(thread());
    pipeline_->setSource(std::move(source));

    if (playback_pos_fn_) {
        pipeline_->setPlaybackPositionProvider(playback_pos_fn_);
    }

    // 注入引擎指针（Pipeline 引用，不拥有）
    pipeline_->setVadEngine(cached_vad_.get());
    pipeline_->setAsrEngine(cached_asr_.get());
    pipeline_->setTranslatorEngine(cached_translator_.get());

    connect(pipeline_.get(), &AsrPipeline::subtitleReady,
            this, &AsrManager::subtitleReady, Qt::QueuedConnection);
    connect(pipeline_.get(), &AsrPipeline::translationReady,
            this, &AsrManager::translationReady, Qt::QueuedConnection);
    connect(pipeline_.get(), &AsrPipeline::engineError,
            this, &AsrManager::engineError, Qt::QueuedConnection);

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
        init_thread_.detach();
        init_cancelling_ = false;
    }

    // 复制参数（audio 流指针在调用期间有效，确保播放器不先析构）
    QString url_copy = url;
    Demuxer::MediaType type_copy = type;
    AVStream* audio_copy = audio;

    init_thread_ = std::thread([this, url_copy, type_copy, audio_copy]() {
        try {
            qDebug() << "[AsrManager] async init started";

            bool ok = false;
            {
                std::lock_guard<std::mutex> lock(init_mtx_);
                if (init_cancelling_) return;

                if (pipeline_) {
                    pipeline_->stop();
                    pipeline_.reset();
                }
                queue_.clear();
                ok = initInternal(url_copy, type_copy, audio_copy);
            }

            if (init_cancelling_) return;

            if (ok) {
                qDebug() << "[AsrManager] async init success, starting pipeline";
                std::lock_guard<std::mutex> lock(init_mtx_);
                if (pipeline_ && !init_cancelling_) pipeline_->start();
            } else {
                qWarning() << "[AsrManager] async init failed";
                emit engineError("ASR init failed");
            }
        } catch (const std::exception& e) {
            qCritical() << "[AsrManager] async init exception:" << e.what();
            emit engineError(QString("ASR init exception: %1").arg(e.what()));
        } catch (...) {
            qCritical() << "[AsrManager] async init unknown exception";
            emit engineError("ASR init unknown exception");
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
        init_thread_.detach();
    }
    init_cancelling_ = false;

    std::lock_guard<std::mutex> lock(init_mtx_);
    if (pipeline_) {
        pipeline_->stop();
        pipeline_.reset();   // 只释放 Pipeline（音频源/线程），引擎缓存保留
    }
    queue_.clear();
}

void AsrManager::releaseEngines() {
    stop();
    std::lock_guard<std::mutex> lock(init_mtx_);
    cached_vad_.reset();
    cached_asr_.reset();
    cached_translator_.reset();
}

void AsrManager::reset() {
    if (pipeline_) pipeline_->reset();
}

void AsrManager::seekTo(double pos_sec) {
    if (pipeline_) {
        pipeline_->seekTo(pos_sec);
    } else {
        // Pipeline 未运行，仅清空队列
        queue_.clear();
    }
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

void AsrManager::applyTranslationToggle(bool enabled) {
    translation_enabled_ = enabled;

    // 如果当前没有运行中的 Pipeline（ASR 未开启或正在初始化），
    // 仅更新配置标志，等下次 init 时自然生效
    if (!pipeline_) {
        qDebug() << "[AsrManager] applyTranslationToggle: no running pipeline, flag only";
        return;
    }

    // 开启翻译：确保翻译引擎已缓存
    if (enabled) {
        // 如果翻译引擎尚未缓存，需要懒加载
        if (!cached_translator_) {
            auto cfg = buildPipelineConfig();
            if (cfg.enable_translation) {
                qDebug() << "[AsrManager] lazy-loading translator on toggle-on";
                cached_translator_ = createTranslator(cfg.translator_type);
                if (cached_translator_ && !cached_translator_->init(cfg.translate_config)) {
                    qDebug() << "[AsrManager] lazy translator init failed, translation disabled";
                    cached_translator_.reset();
                    return;
                } else {
                    cached_translator_type_ = cfg.translator_type;
                    // 注入到现有 Pipeline
                    pipeline_->setTranslatorEngine(cached_translator_.get());
                    pipeline_->setTranslator(cfg.translator_type, cfg.translate_config);
                }
            }
        }
    }

    // 动态启停翻译线程，不重建管线
    pipeline_->enableTranslation(enabled);
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
