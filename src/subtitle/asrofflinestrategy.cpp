#include "asrofflinestrategy.h"
#include <QtConcurrent>
#include <QDebug>

AsrOfflineStrategy::AsrOfflineStrategy() {
    worker_ = std::make_unique<AsrWorker>();
}
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

    AsrConfig cfg;
    cfg.model_path = model_path_.toStdString();

    // 优先从模型缓存获取已加载的上下文
    whisper_context* cached_ctx = nullptr;
    if (AsrModelCache::instance().tryAcquire(cached_ctx)) {
        qDebug() << "[AsrOfflineStrategy] using cached whisper context";
        if (worker_->initWithContext(cached_ctx, cfg)) {
            uses_cached_model_ = true;
            return true;
        }
        AsrModelCache::instance().release();
        qDebug() << "[AsrOfflineStrategy] failed to init with cached context, loading own";
    }

    if (!worker_->init(cfg)) return false;
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
    if (worker_) worker_->release();
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
            std::vector<SubtitleItem> subs;
            worker_->recognize(pcm, subs, start_sec);
            for (auto& s : subs) queue_->push(s);
            start_sec += SEG;
            pcm.clear();
        }
    }

    if (!pcm.empty() && running_) {
        std::vector<SubtitleItem> subs;
        worker_->recognize(pcm, subs, start_sec);
        for (auto& s : subs) queue_->push(s);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    running_ = false;
}
