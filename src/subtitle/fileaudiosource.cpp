#include "fileaudiosource.h"
#include <QDebug>
#include <cmath>

extern "C" {
#include <libavutil/mem.h>
}

FileAudioSource::FileAudioSource(const QString& url)
    : url_(url) {}

FileAudioSource::~FileAudioSource() {
    close();
}

bool FileAudioSource::open() {
    // 1. 打开 Demuxer
    demux_ = std::make_unique<Demuxer>();
if (demux_->open(url_.toStdString().c_str()) < 0) {
        qWarning() << "[FileAudioSource] Failed to open:" << url_;
        return false;
    }

    auto* as = demux_->getStream(AVMEDIA_TYPE_AUDIO);
    if (!as) {
qWarning() << "[FileAudioSource] No audio stream found";
      return false;
    }
    audio_stream_idx_ = demux_->getStreamIndex(AVMEDIA_TYPE_AUDIO);

    // 2. 初始化 Decoder
    dec_ = std::make_unique<Decoder>();
    if (dec_->init(as->codecpar, AVMEDIA_TYPE_AUDIO) < 0) {
        qWarning() << "[FileAudioSource] Decoder init failed";
        return false;
    }

    // 3. 初始化 Resampler（输出 16kHz/mono/float32）
 Resampler::AudioSpec in_spec{}, out_spec{};
    in_spec.sampleRate = as->codecpar->sample_rate;
    in_spec.sampleFmt = (AVSampleFormat)as->codecpar->format;
    in_spec.chs = as->codecpar->ch_layout.nb_channels;
    av_channel_layout_copy(&in_spec.chLayout, &as->codecpar->ch_layout);

    out_spec.sampleRate = 16000;
    out_spec.sampleFmt = AV_SAMPLE_FMT_FLT;
    out_spec.chs = 1;
    av_channel_layout_from_string(&out_spec.chLayout, "mono");

    res_ = std::make_unique<Resampler>();
    if (res_->init(in_spec, out_spec) < 0) {
        qWarning() << "[FileAudioSource] Resampler init failed";
        return false;
    }

    // seek 到开头
    demux_->seek(0);
    dec_->flush();
    current_time_sec_ = 0.0;
    eof_ = false;
    cancelled_ = false;
    pcm_buf_.clear();
  pcm_read_pos_ = 0;

    qDebug() << "[FileAudioSource] Opened:" << url_
         << "sampleRate:" << as->codecpar->sample_rate
             << "channels:" << as->codecpar->ch_layout.nb_channels;
    return true;
}

void FileAudioSource::close() {
    cancelled_ = true;
    pcm_buf_.clear();
  pcm_read_pos_ = 0;

    if (res_) {
        res_->close();
        res_.reset();
    }
    if (dec_) {
   dec_->close();
        dec_.reset();
    }
    if (demux_) {
        demux_->close();
    demux_.reset();
    }
}

int FileAudioSource::pull(float* out, int maxSamples, double& mediaTimeSec) {
    if (cancelled_ || eof_) return 0;

    int written = 0;
    while (written < maxSamples && !cancelled_) {
     // 先从内部缓冲拷贝
   size_t avail = pcm_buf_.size() - pcm_read_pos_;
        if (avail > 0) {
      int to_copy = std::min((int)avail, maxSamples - written);
            std::memcpy(out + written, pcm_buf_.data() + pcm_read_pos_, to_copy * sizeof(float));
            pcm_read_pos_ += to_copy;
  written += to_copy;

   // 缓冲消费完毕，清空
   if (pcm_read_pos_ >= pcm_buf_.size()) {
             pcm_buf_.clear();
                pcm_read_pos_ = 0;
            }
    } else {
  // 缓冲为空，尝试从文件填充
if (!fillBuffer()) {
         eof_ = true;
      break;
            }
      }
    }

    if (written > 0) {
        mediaTimeSec = current_time_sec_;
      current_time_sec_ += (double)written / 16000.0;
    }
    return written;
}

bool FileAudioSource::fillBuffer() {
    if (!demux_ || !dec_ || !res_) return false;

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    bool got_data = false;

    while (!got_data && !cancelled_) {
        int ret = demux_->readPacket(pkt);
        if (ret < 0) {
        // EOF 或读取错误
    av_packet_free(&pkt);
       av_frame_free(&frame);
            return false;
        }

 if (pkt->stream_index != audio_stream_idx_) {
    av_packet_unref(pkt);
 continue;
        }

        if (dec_->decode(pkt, frame) != 0) {
            av_packet_unref(pkt);
       continue;
        }

        // 重采样
        uint8_t* buf = (uint8_t*)av_malloc(res_->outputBufferSize(frame->nb_samples));
        int samples = 0;
        if (res_->resample(frame, &buf, &samples) >= 0 && samples > 0) {
            pcm_buf_.insert(pcm_buf_.end(), (float*)buf, (float*)buf + samples);
            pcm_read_pos_ = 0;
   got_data = true;
        }
        av_free(buf);
      av_packet_unref(pkt);
        av_frame_unref(frame);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    return got_data;
}

AVStream* FileAudioSource::audioStream() const {
    if (demux_) {
   return demux_->getStream(AVMEDIA_TYPE_AUDIO);
    }
    return nullptr;
}

bool FileAudioSource::seekTo(double pos_sec) {
    if (!demux_ || !dec_) return false;

    // seek 内部 demuxer 到目标位置（微秒）
    int64_t pos_us = (int64_t)(pos_sec * 1000000.0);
    int ret = demux_->seek(pos_us);
    if (ret < 0) {
        qWarning() << "[FileAudioSource] seek failed:" << pos_sec << "s";
        return false;
    }

    // flush decoder 缓存
    dec_->flush();

    // 清空 PCM 缓冲，重置时间
    pcm_buf_.clear();
    pcm_read_pos_ = 0;
    current_time_sec_ = pos_sec;
    eof_ = false;

    qDebug() << "[FileAudioSource] seeked to" << pos_sec << "s";
    return true;
}
