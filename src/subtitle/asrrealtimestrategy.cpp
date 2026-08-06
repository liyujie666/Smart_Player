#include "asrrealtimestrategy.h"
#include "whisperengine.h"
#include "asrmodelcache.h"
#include "utils/asrutils.h"
#include <QDebug>
#include <chrono>
#include <algorithm>
#include <cmath>

AsrRealtimeStrategy::AsrRealtimeStrategy() = default;
AsrRealtimeStrategy::~AsrRealtimeStrategy() {
    stop();
    release();
}

bool AsrRealtimeStrategy::init(const QString&, AVStream* audio, SubtitleQueue* queue) {
    queue_ = queue;
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
    if (resampler_->init(in_spec, out_spec) < 0) return false;

    // ===== ASR 引擎初始化 =====
    AsrEngineConfig cfg;
    cfg.model_path = model_path_.toStdString();

    engine_ = createAsrEngine(engine_type_);
    if (!engine_) return false;

    if (engine_type_ == AsrEngineType::Whisper) {
        auto* whisper = dynamic_cast<WhisperEngine*>(engine_.get());
        whisper_context* cached_ctx = nullptr;
        if (whisper && AsrModelCache::instance().tryAcquire(cached_ctx)) {
            qDebug() << "[AsrRealtimeStrategy] using cached whisper context";
            if (whisper->initWithContext(cached_ctx, cfg)) {
                uses_cached_model_ = true;
                return true;
            }
            AsrModelCache::instance().release();
            qDebug() << "[AsrRealtimeStrategy] failed to init with cached context, loading own";
        }
    }

    if (!engine_->init(cfg)) return false;

    // ===== VAD 引擎初始化 =====
    if (vad_enabled_ && !vad_model_path_.isEmpty()) {
        vad_ = createVadEngine(VadEngineType::FSMN);
        if (vad_) {
            VadConfig vcfg;
            vcfg.model_path = vad_model_path_.toStdString();
            if (!vad_->init(vcfg)) {
                qWarning() << "[AsrRealtimeStrategy] VAD init failed";
                vad_.reset();
            }
        }
    }

    // ===== 翻译引擎初始化 =====
    if (translation_enabled_) {
        translator_ = createTranslator(translator_type_);
        if (translator_ && !translator_->init(translate_config_)) {
            qWarning() << "[AsrRealtimeStrategy] Translator init failed";
            translator_.reset();
        }
    }

    return true;
}

void AsrRealtimeStrategy::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread(&AsrRealtimeStrategy::run, this);
}

void AsrRealtimeStrategy::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    ring_.clear();
    last_text_.clear();
}

void AsrRealtimeStrategy::reset() {
    ring_.clear();
    last_text_.clear();
    if (vad_) vad_->reset();
}

void AsrRealtimeStrategy::release() {
    if (engine_) engine_->release();
    if (vad_) vad_->release();
    if (translator_) translator_->release();
    if (uses_cached_model_) {
        AsrModelCache::instance().release();
        uses_cached_model_ = false;
    }
    resampler_.reset();
}

void AsrRealtimeStrategy::sendAudio(AVFrame* frame) {
    if (!resampler_ || !frame) return;
    uint8_t* buf = (uint8_t*)av_malloc(resampler_->outputBufferSize(frame->nb_samples));
    int samples = 0;
    if (resampler_->resample(frame, &buf, &samples) >= 0 && samples > 0) {
        double pts = frame->pts * av_q2d(tb_);
        ring_.push((float*)buf, samples, pts);
    }
    av_freep(&buf);
}

void AsrRealtimeStrategy::run() {
    const int SR = 16000;
    const size_t win = 3 * SR;    // 3s 窗口
    const size_t step = 1 * SR;   // 1s 步进
    std::vector<float> buf(win);

    while (running_) {
        if (ring_.available() < win) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        double start_time = ring_.head_time_sec();
        ring_.peek(buf.data(), win);

        if (vad_ && vad_->isReady()) {
            // ===== VAD 模式：检测语音段，逐段做 ASR =====
            auto segments = vad_->process(buf, start_time);
            for (const auto& seg : segments) {
                int start_sample = (int)((seg.start_sec - start_time) * SR);
                int end_sample = (int)((seg.end_sec - start_time) * SR);
                start_sample = std::max(0, start_sample);
                end_sample = std::min((int)buf.size(), end_sample);

                if (end_sample <= start_sample) continue;

                std::vector<float> speech_pcm(buf.begin() + start_sample, buf.begin() + end_sample);
                std::vector<SubtitleItem> res;
                if (engine_->recognize(speech_pcm, res, seg.start_sec)) {
                    std::string text;
                    for (auto& i : res) text = AsrUtil::mergeOverlap(text, i.text);
                    if (!text.empty() && text != last_text_) {
                        last_text_ = text;
                        SubtitleItem item;
                        item.text = text;
                        item.start_sec = res.front().start_sec;
                        item.end_sec = res.back().end_sec;

                        // 翻译
                        if (translator_ && translator_->isReady()) {
                            auto tr = translator_->translate(item.text);
                            if (tr.success) {
                                item.translated_text = tr.translated_text;
                            }
                        }

                        queue_->push(item);
                    }
                }
            }
        } else {
            // ===== 无 VAD：直接做 ASR =====
            std::vector<SubtitleItem> res;
            if (engine_->recognize(buf, res, start_time)) {
                std::string text;
                for (auto& i : res) text = AsrUtil::mergeOverlap(text, i.text);
                if (!text.empty() && text != last_text_) {
                    last_text_ = text;
                    SubtitleItem item;
                    item.text = text;
                    item.start_sec = res.front().start_sec;
                    item.end_sec = res.back().end_sec;

                    // 翻译
                    if (translator_ && translator_->isReady()) {
                        auto tr = translator_->translate(item.text);
                        if (tr.success) {
                            item.translated_text = tr.translated_text;
                        }
                    }

                    queue_->push(item);
                }
            }
        }

        ring_.consume(step);
    }
}
