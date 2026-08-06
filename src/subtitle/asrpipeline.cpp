#include "asrpipeline.h"
#include "whisperengine.h"
#include "asrmodelcache.h"
#include "utils/asrutils.h"
#include <QDebug>
#include <chrono>

AsrPipeline::AsrPipeline(QObject* parent) : QObject(parent) {}

AsrPipeline::~AsrPipeline() {
    stop();
}

bool AsrPipeline::init(const PipelineConfig& cfg, AVStream* audio, SubtitleQueue* queue) {
    config_ = cfg;
    queue_ = queue;

    // 初始化重采样器（如果有音频流参数）
    if (audio) {
        tb_ = audio->time_base;

        Resampler::AudioSpec in_spec, out_spec;
        in_spec.sampleRate = audio->codecpar->sample_rate;
        in_spec.sampleFmt = (AVSampleFormat)audio->codecpar->format;
        in_spec.chs = audio->codecpar->ch_layout.nb_channels;
        av_channel_layout_copy(&in_spec.chLayout, &audio->codecpar->ch_layout);

        out_spec.sampleRate = 16000;
        out_spec.sampleFmt = AV_SAMPLE_FMT_FLT;
        out_spec.chs = 1;
        av_channel_layout_from_string(&out_spec.chLayout, "mono");

        resampler_ = std::make_unique<Resampler>();
        if (resampler_->init(in_spec, out_spec)< 0) {
            emit engineError("Resampler init failed");
            return false;
        }
    }

    // 初始化 VAD
    if (config_.enable_vad) {
        vad_ = createVadEngine(config_.vad_type);
        if (vad_ && !vad_->init(config_.vad_config)) {
            qDebug() << "[AsrPipeline] VAD init failed, falling back to no-VAD mode";
            vad_.reset();
            config_.enable_vad = false;
        }
    }

    // 初始化 ASR引擎
    asr_ = createAsrEngine(config_.asr_type);
    if (!asr_) {
        emit engineError("Failed to create ASR engine");
        return false;
    }

    // Whisper 特有：优先使用缓存的context
    if (config_.asr_type == AsrEngineType::Whisper) {
        auto* whisper = dynamic_cast<WhisperEngine*>(asr_.get());
        whisper_context* cached_ctx = nullptr;
        if (whisper && AsrModelCache::instance().tryAcquire(cached_ctx)) {
            qDebug() << "[AsrPipeline] using cached whisper context";
            if (whisper->initWithContext(cached_ctx, config_.asr_config)) {
                uses_cached_model_ = true;
            } else {
                AsrModelCache::instance().release();
                if (!asr_->init(config_.asr_config)) {
                    emit engineError("ASR engine init failed");
                    return false;
                }
            }
        } else {
            if (!asr_->init(config_.asr_config)) {
                emit engineError("ASR engine init failed");
                return false;
            }
        }
    } else {
        if (!asr_->init(config_.asr_config)) {
            emit engineError("ASR engine init failed");
            return false;
        }
    }

    // 初始化翻译引擎（可选）
    if (config_.enable_translation) {
        translator_ = createTranslator(config_.translator_type);
        if (translator_ && !translator_->init(config_.translate_config)) {
            qDebug() << "[AsrPipeline] Translator init failed, translation disabled";
            translator_.reset();
            config_.enable_translation = false;
        }
    }

    return true;
}

void AsrPipeline::start() {
    if (running_.exchange(true)) return;

    vad_asr_thread_ = std::thread(&AsrPipeline::vadAsrLoop, this);

    if (config_.enable_translation && translator_ && translator_->isReady()) {
        translate_running_ = true;
        translate_thread_ = std::thread(&AsrPipeline::translateLoop, this);
    }
}

void AsrPipeline::stop() {
    running_ = false;
    if (vad_asr_thread_.joinable()) vad_asr_thread_.join();

    // 停止翻译线程
    translate_running_ = false;
    translate_cv_.notify_all();
    if (translate_thread_.joinable()) translate_thread_.join();

    // 清理
    ring_.clear();
    last_text_.clear();
    {
        std::lock_guard<std::mutex> lock(translate_mtx_);
        translate_queue_ = {};
    }
}

void AsrPipeline::reset() {
    ring_.clear();
    last_text_.clear();
    if (vad_) vad_->reset();
    if (asr_) asr_->reset();
    {
        std::lock_guard<std::mutex> lock(translate_mtx_);
        translate_queue_ = {};
    }
}

void AsrPipeline::feedAudio(AVFrame* frame) {
    if (!resampler_ || !frame) return;

    uint8_t* buf = (uint8_t*)av_malloc(resampler_->outputBufferSize(frame->nb_samples));
    int samples = 0;
    if (resampler_->resample(frame, &buf, &samples) >= 0 && samples > 0) {
        double pts = frame->pts * av_q2d(tb_);
        ring_.push((float*)buf, samples, pts);
    }
    av_freep(&buf);
}

void AsrPipeline::feedPcm(const std::vector<float>& pcm, double base_sec) {
    if (!asr_ || !asr_->isReady()) return;

    if (config_.enable_vad && vad_ && vad_->isReady()) {
        auto segments = vad_->process(pcm, base_sec);
        processVadSegments(segments, pcm, base_sec);
    } else {
        processDirectAsr(pcm, base_sec);
    }
}

void AsrPipeline::setAsrEngine(AsrEngineType type, const AsrEngineConfig& cfg) {
    // 先释放旧引擎
    if (asr_) asr_->release();
    if (uses_cached_model_) {
        AsrModelCache::instance().release();
        uses_cached_model_ = false;
    }

    config_.asr_type = type;
    config_.asr_config = cfg;

    asr_ = createAsrEngine(type);
    if (asr_) {
        if (!asr_->init(cfg)) {
            emit engineError(QString("Failed to init %1 engine")
                                .arg(QString::fromStdString(asr_->name())));
        }
    }
}

void AsrPipeline::setTranslator(TranslatorType type, const TranslateConfig& cfg) {
    if (translator_) translator_->release();

    config_.translator_type = type;
    config_.translate_config = cfg;

    translator_ = createTranslator(type);
    if (translator_) {
        if (!translator_->init(cfg)) {
            emit engineError(QString("Failed to init %1 translator")
                                 .arg(QString::fromStdString(translator_->name())));
        }
    }
}

void AsrPipeline::enableTranslation(bool enable) {
    config_.enable_translation = enable;

    if (enable && translator_ && translator_->isReady() && !translate_running_) {
        translate_running_ = true;
        translate_thread_ = std::thread(&AsrPipeline::translateLoop, this);
    } else if (!enable) {
        translate_running_ = false;
        translate_cv_.notify_all();
        if (translate_thread_.joinable()) translate_thread_.join();
    }
}

void AsrPipeline::enableVad(bool enable) {
    config_.enable_vad = enable;
    if (enable && !vad_) {
        vad_ = createVadEngine(config_.vad_type);
        if (vad_) vad_->init(config_.vad_config);
    }
}

std::string AsrPipeline::currentAsrEngineName() const {
    return asr_ ? asr_->name() : "None";
}

std::string AsrPipeline::currentTranslatorName() const {
    return translator_ ? translator_->name() : "None";
}

//========== 内部线程 ==========

void AsrPipeline::vadAsrLoop() {
    const int SR = 16000;
    const size_t win = 3* SR;    // 3s 窗口
    const size_t step = 1 * SR;   // 1s 步进
    std::vector<float> buf(win);

    while (running_) {
        if (ring_.available()< win) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        double start_time = ring_.head_time_sec();
        ring_.peek(buf.data(), win);

        if (config_.enable_vad && vad_ && vad_->isReady()) {
            auto segments = vad_->process(buf, start_time);
            if (!segments.empty()) {
                processVadSegments(segments, buf, start_time);
            }
        } else {
            // 无VAD，直接做ASR
            processDirectAsr(buf, start_time);
        }

        ring_.consume(step);
    }

    // 刷新 VAD 残余
    if (vad_ && vad_->isReady()) {
        auto segments = vad_->flush();
        if (!segments.empty() && !buf.empty()) {
            // flush阶段用最后缓冲的数据
            processVadSegments(segments, buf, ring_.head_time_sec());
        }
    }
}

void AsrPipeline::processVadSegments(const std::vector<VadSegment>& segments,
                                      const std::vector<float>& pcm,
                                      double base_sec) {
    const int SR = 16000;

    for (const auto& seg : segments) {
        // 从PCM中截取VAD检测到的语音段
        int start_sample = (int)((seg.start_sec - base_sec) * SR);
        int end_sample = (int)((seg.end_sec - base_sec) * SR);

        start_sample = std::max(0, start_sample);
        end_sample = std::min((int)pcm.size(), end_sample);

        if (end_sample <= start_sample) continue;

        std::vector<float> speech_pcm(pcm.begin() + start_sample, pcm.begin() + end_sample);

        std::vector<SubtitleItem> results;
        if (asr_->recognize(speech_pcm, results, seg.start_sec)) {
            for (auto& item : results) {
                if (item.text != last_text_) {
                    last_text_ = item.text;
                    queue_->push(item);
                emit subtitleReady(item);

                    // 送入翻译队列
                    if (config_.enable_translation && translator_) {
                        std::lock_guard<std::mutex> lock(translate_mtx_);
                        translate_queue_.push(item);
                        translate_cv_.notify_one();
                    }
                }
            }
        }
    }
}

void AsrPipeline::processDirectAsr(const std::vector<float>& pcm, double base_sec) {
    std::vector<SubtitleItem> results;
    if (asr_->recognize(pcm, results, base_sec)) {
        std::string text;
        for (auto& i : results) text = AsrUtil::mergeOverlap(text, i.text);

        if (!text.empty() && text != last_text_) {
            last_text_ = text;
            SubtitleItem item;
            item.text = text;
            item.start_sec = results.front().start_sec;
            item.end_sec = results.back().end_sec;
            queue_->push(item);
            emit subtitleReady(item);

            // 送入翻译队列
            if (config_.enable_translation && translator_) {
                std::lock_guard<std::mutex> lock(translate_mtx_);
                translate_queue_.push(item);
                translate_cv_.notify_one();
            }
        }
    }
}

void AsrPipeline::translateLoop() {
    while (translate_running_) {
        SubtitleItem item;
        {
            std::unique_lock<std::mutex> lock(translate_mtx_);
            translate_cv_.wait(lock, [this] {
                return !translate_queue_.empty() || !translate_running_;
            });
            if (!translate_running_) break;
            if (translate_queue_.empty()) continue;
            item = translate_queue_.front();
            translate_queue_.pop();
        }

        if (!translator_ || !translator_->isReady()) continue;

        auto result = translator_->translate(item.text);
        if (result.success) {
            item.translated_text = result.translated_text;
            // 更新字幕队列中的翻译结果
            queue_->push(item);
            emit translationReady(item);
        } else {
            qDebug() << "[AsrPipeline] Translation failed:" << QString::fromStdString(result.error_msg);
        }
    }
}
