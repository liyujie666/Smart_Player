#include "asrofflinestrategy.h"
#include "whisperengine.h"
#include "utils/asrutils.h"
#include <QtConcurrent>
#include <QDebug>
#include <cmath>
#include <algorithm>

AsrOfflineStrategy::AsrOfflineStrategy() = default;
AsrOfflineStrategy::~AsrOfflineStrategy() {
    stop();
    release();
}

bool AsrOfflineStrategy::init(const QString& url, AVStream*, SubtitleQueue* queue) {
    url_ = url;
    queue_ = queue;

    demux_ = std::make_unique<Demuxer>();
    if (demux_->open(url.toStdString().c_str()) < 0) return false;
    auto as = demux_->getStream(AVMEDIA_TYPE_AUDIO);
    if (!as) return false;

    dec_ = std::make_unique<Decoder>();
    if (dec_->init(as->codecpar, AVMEDIA_TYPE_AUDIO) < 0) return false;

    Resampler::AudioSpec in_spec, out_spec;
    in_spec.sampleRate = as->codecpar->sample_rate;
    in_spec.sampleFmt = (AVSampleFormat)as->codecpar->format;
    in_spec.chs = as->codecpar->ch_layout.nb_channels;
    av_channel_layout_copy(&in_spec.chLayout, &as->codecpar->ch_layout);

    out_spec.sampleRate = 16000;
    out_spec.sampleFmt = AV_SAMPLE_FMT_FLT;
    out_spec.chs = 1;
    av_channel_layout_from_string(&out_spec.chLayout, "mono");

    res_ = std::make_unique<Resampler>();
    if (res_->init(in_spec, out_spec) < 0) return false;

    // ===== ASR 引擎初始化 =====
    AsrEngineConfig cfg;
    cfg.model_path = model_path_.toStdString();

    engine_ = createAsrEngine(engine_type_);
    if (!engine_) return false;

    // Whisper 特有：优先从模型缓存获取
    if (engine_type_ == AsrEngineType::Whisper) {
        auto* whisper = dynamic_cast<WhisperEngine*>(engine_.get());
        whisper_context* cached_ctx = nullptr;
        if (whisper && AsrModelCache::instance().tryAcquire(cached_ctx)) {
            qDebug() << "[AsrOfflineStrategy] using cached whisper context";
            if (whisper->initWithContext(cached_ctx, cfg)) {
                uses_cached_model_ = true;
            } else {
                AsrModelCache::instance().release();
                if (!engine_->init(cfg)) return false;
            }
        } else {
            if (!engine_->init(cfg)) return false;
        }
    } else {
        if (!engine_->init(cfg)) return false;
    }

    // ===== VAD 引擎初始化 =====
    if (vad_enabled_ && !vad_model_path_.isEmpty()) {
        vad_ = createVadEngine(VadEngineType::FSMN);
        if (vad_) {
            VadConfig vcfg;
            vcfg.model_path = vad_model_path_.toStdString();
            if (!vad_->init(vcfg)) {
                qWarning() << "[AsrOfflineStrategy] VAD init failed, running without VAD";
                vad_.reset();
            }
        }
    }

    // ===== 翻译引擎初始化 =====
    if (translation_enabled_) {
        translator_ = createTranslator(translator_type_);
        if (translator_ && !translator_->init(translate_config_)) {
            qWarning() << "[AsrOfflineStrategy] Translator init failed";
            translator_.reset();
        }
    }

    return true;
}

void AsrOfflineStrategy::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread(&AsrOfflineStrategy::run, this);
}

void AsrOfflineStrategy::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}
void AsrOfflineStrategy::reset() {

}
void AsrOfflineStrategy::release() {
    if (engine_) engine_->release();
    if (vad_) vad_->release();
    if (translator_) translator_->release();
    if (uses_cached_model_) {
        AsrModelCache::instance().release();
        uses_cached_model_ = false;
    }
    res_.reset();
    if (dec_) {
        dec_->close();
        dec_.reset();
    }
    if (demux_) {
        demux_->close();
        demux_.reset();
    }
}

// 翻译辅助函数（在 ASR 线程中异步调用翻译）
static void translateIfNeeded(const std::vector<SubtitleItem>& subs,
                               std::unique_ptr<ITranslator>& translator,
                               SubtitleQueue* queue) {
    if (!translator || !translator->isReady()) return;

    for (const auto& sub : subs) {
        if (sub.text.empty()) continue;
        auto result = translator->translate(sub.text);
        if (result.success) {
            // 创建带译文的副本 push 到队列
            SubtitleItem translated = sub;
            translated.translated_text = result.translated_text;
            queue->push(translated);
        }
    }
}

void AsrOfflineStrategy::run() {
    const int SEG = 30, SR = 16000;
    std::vector<float> pcm;
    double start_sec = 0;
    auto audio_idx = demux_->getStreamIndex(AVMEDIA_TYPE_AUDIO);

    demux_->seek(0);
    dec_->flush();
    queue_->clear();

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    while (demux_->readPacket(pkt) >= 0 && running_) {
        if (pkt->stream_index != audio_idx) { av_packet_unref(pkt); continue; }
        if (dec_->decode(pkt, frame) != 0) { av_packet_unref(pkt); continue; }

        uint8_t* buf = (uint8_t*)av_malloc(res_->outputBufferSize(frame->nb_samples));
        int samples = 0;
        if (res_->resample(frame, &buf, &samples) >= 0 && samples > 0) {
            pcm.insert(pcm.end(), (float*)buf, (float*)buf + samples);
        }
        av_free(buf);
        av_packet_unref(pkt);
        av_frame_unref(frame);

        if (pcm.size() >= SEG * SR) {
            processAudioChunk(pcm, start_sec);
            start_sec += SEG;
            pcm.clear();
        }
    }

    if (!pcm.empty() && running_) {
        processAudioChunk(pcm, start_sec);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    running_ = false;
}

void AsrOfflineStrategy::processAudioChunk(const std::vector<float>& pcm, double base_sec) {
    if (vad_ && vad_->isReady()) {
        // VAD 模式：先做语音分段，再对每段做 ASR
        auto segments = vad_->process(pcm, base_sec);
        for (const auto& seg : segments) {
            int start_sample = (int)((seg.start_sec - base_sec) * 16000);
            int end_sample = (int)((seg.end_sec - base_sec) * 16000);
            start_sample = std::max(0, start_sample);
            end_sample = std::min((int)pcm.size(), end_sample);

            if (end_sample <= start_sample) continue;

            std::vector<float> speech_pcm(pcm.begin() + start_sample, pcm.begin() + end_sample);
            std::vector<SubtitleItem> subs;
            if (engine_->recognize(speech_pcm, subs, seg.start_sec)) {
                for (auto& s : subs) queue_->push(s);
            }
        }

        // 翻译
        if (translator_) {
            std::vector<SubtitleItem> all_subs;
            // 注意: 翻译是异步的，这里简化为同步调用
            translateIfNeeded(all_subs, translator_, queue_);
        }
    } else {
        // 无 VAD：直接 ASR
        std::vector<SubtitleItem> subs;
        engine_->recognize(pcm, subs, base_sec);
        for (auto& s : subs) queue_->push(s);

        if (translator_) {
            translateIfNeeded(subs, translator_, queue_);
        }
    }
}
