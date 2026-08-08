#include "liveaudiosource.h"
#include <QDebug>

extern "C" {
#include <libavutil/mem.h>
}

LiveAudioSource::LiveAudioSource(AVStream* audio)
    : audio_stream_(audio)
    , ring_(160000, 16000)  // 10s 容量, 16kHz
{
  if (audio) {
        tb_ = audio->time_base;
    }
}

LiveAudioSource::~LiveAudioSource() {
    close();
}

bool LiveAudioSource::open() {
    if (!audio_stream_) {
        qWarning() << "[LiveAudioSource] No audio stream provided";
        return false;
 }

    // 初始化 Resampler（输出 16kHz/mono/float32）
    Resampler::AudioSpec in_spec{}, out_spec{};
    in_spec.sampleRate = audio_stream_->codecpar->sample_rate;
    in_spec.sampleFmt = (AVSampleFormat)audio_stream_->codecpar->format;
 in_spec.chs = audio_stream_->codecpar->ch_layout.nb_channels;
    av_channel_layout_copy(&in_spec.chLayout, &audio_stream_->codecpar->ch_layout);

    out_spec.sampleRate = 16000;
    out_spec.sampleFmt = AV_SAMPLE_FMT_FLT;
    out_spec.chs = 1;
    av_channel_layout_from_string(&out_spec.chLayout, "mono");

    resampler_ = std::make_unique<Resampler>();
    if (resampler_->init(in_spec, out_spec) < 0) {
        qWarning() << "[LiveAudioSource] Resampler init failed";
        return false;
    }

    cancelled_ = false;
    ring_.clear();

    qDebug() << "[LiveAudioSource] Opened, input sampleRate:"
  << audio_stream_->codecpar->sample_rate;
    return true;
}

void LiveAudioSource::close() {
    cancelled_ = true;
    ring_.clear();
    if (resampler_) {
   resampler_->close();
        resampler_.reset();
    }
}

void LiveAudioSource::pushFrame(AVFrame* frame) {
    if (!resampler_ || !frame || cancelled_) return;

    uint8_t* buf = (uint8_t*)av_malloc(resampler_->outputBufferSize(frame->nb_samples));
    int samples = 0;
    if (resampler_->resample(frame, &buf, &samples) >= 0 && samples > 0) {
        double pts = frame->pts * av_q2d(tb_);
        ring_.push((float*)buf, samples, pts);
    }
    av_freep(&buf);
}

size_t LiveAudioSource::available() const {
    return ring_.available();
}

void LiveAudioSource::peek(float* out, size_t n) const {
    ring_.peek(out, n);
}

double LiveAudioSource::headTimeSec() const {
    return ring_.head_time_sec();
}

void LiveAudioSource::consume(size_t n) {
  ring_.consume(n);
}

void LiveAudioSource::clear() {
    ring_.clear();
}

bool LiveAudioSource::seekTo(double pos_sec) {
    // Push 模式：播放器的 demuxer 已 seek，新帧会从正确位置送入
    // 只需清空内部 ring buffer 中的旧数据
    (void)pos_sec;
    ring_.clear();
    qDebug() << "[LiveAudioSource] seekTo" << pos_sec << "s, ring cleared";
    return true;
}
