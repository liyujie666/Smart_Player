#include "asrrealtimestrategy.h"
#include "asrworker.h"
#include "utils/asrutils.h"
#include <QDebug>
#include <chrono>

AsrRealtimeStrategy::AsrRealtimeStrategy() {
    worker_ = std::make_unique<AsrWorker>();
}
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

    AsrConfig cfg;
    cfg.model_path = model_path_.toStdString();
    // cfg.language = "zh";
    // cfg.translate = true;
    return worker_->init(cfg);
}

void AsrRealtimeStrategy::start() {
    if (running_) return;
    running_ = true;
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
}
void AsrRealtimeStrategy::release() {
    resampler_.reset();
    worker_->release();
}

void AsrRealtimeStrategy::sendAudio(AVFrame* frame) {
    if (!resampler_ || !frame) return;
    uint8_t* buf = (uint8_t*)av_malloc(resampler_->getOutputBufferSize(frame->nb_samples));
    int samples = 0;
    if (resampler_->resample(frame, &buf, &samples) >= 0 && samples > 0) {
        double pts = frame->pts * av_q2d(tb_);
        ring_.push((float*)buf, samples, pts);
    }
    av_free(buf);
}

void AsrRealtimeStrategy::run() {
    const int SR = 16000;
    const size_t win = 3 * SR;
    const size_t step = 1 * SR;
    std::vector<float> buf(win);

    while (running_) {
        if (ring_.available() < win) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        double start = ring_.head_time_sec();
        ring_.peek(buf.data(), win);

        std::vector<SubtitleItem> res;
        if (worker_->recognize(buf, res, start)) {
            std::string text;
            for (auto& i : res) text = AsrUtil::mergeOverlap(text, i.text);
            if (!text.empty() && text != last_text_) {
                last_text_ = text;
                SubtitleItem item;
                item.text = text;
                item.start_sec = res.front().start_sec;
                item.end_sec = res.back().end_sec;
                queue_->push(item);
            }
        }
        ring_.consume(step);
    }
}
