#include "audiooutput.h"
#include "syncclock.h"
#include "pool/gloabalpool.h"
#include <QDebug>
#include <cstring>
#include <cmath>
#include <SDL2/SDL.h>

#define LOG_DEBUG qDebug() << "[AudioOutput] " // 统一日志前缀

AudioOutput::AudioOutput(const Resampler::AudioSpec &inSpec,
                         const Resampler::AudioSpec &outSpec,
                         AVFrameQueue *frameQueue,
                         AVSyncClock* sync_clock)
    : sync_clock_(sync_clock),in_spec_(inSpec), out_spec_(outSpec), frame_queue_(frameQueue)
{
    // need_resample_ = !(
    //     in_spec_.sampleRate == out_spec_.sampleRate &&
    //     in_spec_.sampleFmt == out_spec_.sampleFmt &&
    //     in_spec_.chs == out_spec_.chs &&
    //     av_channel_layout_compare(&in_spec_.chLayout, &out_spec_.chLayout) == 0
    //     );
    // LOG_DEBUG << "是否需要重采样: " << need_resample_;

    if(need_resample_){
        resampler_ = new Resampler();
        resampler_->init(inSpec, outSpec);
        LOG_DEBUG << "重采样器初始化完成";
    } else {
        resampler_ = nullptr;
    }

    sample_rate_ = outSpec.sampleRate;

    // 初始化缓冲区（防止野指针）
    audio_buf_ = nullptr;
    audio_buf_size_ = 0;
    audio_buf_index_ = 0;
}

AudioOutput::~AudioOutput()
{
    DeInit();
    if (resampler_) {
        delete resampler_;
        resampler_ = nullptr;
    }

}

int AudioOutput::Init()
{
    if (is_audio_init_) {
        return 0;
    }

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        qDebug() << "SDL初始化失败: " << SDL_GetError();
        return -1;
    }

    SDL_AudioSpec spec{};
    spec.freq = out_spec_.sampleRate;
    spec.channels = out_spec_.chs;
    spec.samples = 1024;
    spec.callback = fill_audio_pcm;
    spec.userdata = this;

    if (out_spec_.sampleFmt == AV_SAMPLE_FMT_S16) {
        spec.format = AUDIO_S16LSB;
    } else if (out_spec_.sampleFmt == AV_SAMPLE_FMT_FLT) {
        spec.format = AUDIO_F32LSB;
    } else {
        qDebug() << "不支持的采样格式: " << out_spec_.sampleFmt;
        return -1;
    }

    if (SDL_OpenAudio(&spec, nullptr) != 0) {
        qDebug() << "SDL打开音频设备失败: " << SDL_GetError();
        return -1;
    }

    SDL_PauseAudio(0);
    is_audio_init_ = true;
    return 0;
}

int AudioOutput::DeInit()
{
    if (!is_audio_init_) {
        return 0;
    }

    SDL_PauseAudio(1);
    SDL_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    if (audio_buf_) {
        free(audio_buf_);
        audio_buf_ = nullptr;
    }

    audio_buf_size_ = 0;
    audio_buf_index_ = 0;
    is_audio_init_ = false;
    return 0;
}

void AudioOutput::pause()
{
    SDL_PauseAudio(1);
}

void AudioOutput::resume()
{
    SDL_PauseAudio(0);
}

void AudioOutput::setAudioTimebase(AVRational tb)
{
    audio_timebase_ = tb;
}


void AudioOutput::setVolume(int volume) {
    volume_ = qBound(0, volume, 100);
}

int AudioOutput::volume() const {
    return volume_;
}

void AudioOutput::setMute(bool mute) {
    mute_ = mute;
}

bool AudioOutput::isMute() const {
    return mute_;
}
int64_t AudioOutput::audioClock() const
{
    QMutexLocker locker(&mutex_);
    return audio_clock_us_;
}

int AudioOutput::resampleFrameToBuffer(uint8_t* stream, int len)
{
    if (!frame_queue_) {
        memset(stream, 0, len);
        return -1;
    }
    if (!is_audio_init_) {
        memset(stream, 0, len);
        return -1;
    }

    memset(stream, 0, len);
    int len_remaining = len;

    while (len_remaining > 0) {
        if (audio_buf_index_ >= audio_buf_size_) {
            AVFrame* frame = frame_queue_->Pop(10);
            if (!frame) {
                return 0;
            }

            if (frame->nb_samples <= 0 || frame->format == AV_SAMPLE_FMT_NONE) {
                //av_frame_unref(frame);
                GlobalPool::getFramePool().recycle(frame);
                continue;
            }
            if (need_resample_) {
                if(!resampler_){
                    GlobalPool::getFramePool().recycle(frame);
                    return 0;
                }
                int out_samples = 0;
                int buf_size = resampler_->outputBufferSize(frame->nb_samples);

                uint8_t* tmp = (uint8_t*)realloc(audio_buf_, buf_size);
                if(!tmp){
                    free(audio_buf_);
                    audio_buf_ = nullptr;
                    GlobalPool::getFramePool().recycle(frame);
                    return 0;
                }
                audio_buf_ = tmp;

                resampler_->resample(frame, &audio_buf_, &out_samples);
                audio_buf_size_ = resampler_->outputBufferSize(out_samples);
            } else {
                audio_buf_size_ = av_samples_get_buffer_size(
                    nullptr, frame->ch_layout.nb_channels,
                    frame->nb_samples, (AVSampleFormat)frame->format, 1
                    );
                if(audio_buf_size_ <=0){
                    GlobalPool::getFramePool().recycle(frame);
                    return 0;
                }

                uint8_t* tmp = (uint8_t*)realloc(audio_buf_, audio_buf_size_);
                if(!tmp){
                    free(audio_buf_);
                    audio_buf_ = nullptr;
                    GlobalPool::getFramePool().recycle(frame);
                    return 0;
                }
                audio_buf_ = tmp;

                if(!frame->data[0]){
                    GlobalPool::getFramePool().recycle(frame);
                    return 0;
                }
                memcpy(audio_buf_, frame->data[0], audio_buf_size_);
            }

            audio_buf_index_ = 0;
            {
                QMutexLocker locker(&mutex_);
                audio_clock_us_ = av_rescale_q(frame->pts, audio_timebase_, {1, 1000000});
            }
            sync_clock_->set_audio_clock(audio_clock_us_);


            GlobalPool::getFramePool().recycle(frame);
        }

        int copy_len = qMin(len_remaining, (int)(audio_buf_size_ - audio_buf_index_));
        if(!audio_buf_){
            return 0;
        }
        memcpy(stream, audio_buf_ + audio_buf_index_, copy_len);

        // 音量调节
        applyVolume(stream, copy_len);

        stream += copy_len;
        len_remaining -= copy_len;
        audio_buf_index_ += copy_len;
    }

    return 0;
}

void AudioOutput::applyVolume(uint8_t* data, int len) {
    QMutexLocker locker(&mutex_);
    if (!data || len <= 0) return;

    // 静音：直接填0
    if (mute_) {
        memset(data, 0, len);
        return;
    }

    // 音量系数：0~100 → 0.0~1.0
    float factor = volume_ / 100.0f;
    if (factor >= 1.0f) return; //

    // 16位采样格式：2字节1个采样
    int sample_count = len / 2;
    int16_t* samples = (int16_t*)data;

    // 逐采样缩放，防止溢出
    for (int i = 0; i < sample_count; i++) {
        samples[i] = static_cast<int16_t>(samples[i] * factor);
    }
}

void AudioOutput::fill_audio_pcm(void *udata, Uint8 *stream, int len)
{
    AudioOutput* self = (AudioOutput*)udata;
    if (!self) {
        return;
    }
    if (!self->is_audio_init_) {
        return;
    }

    self->resampleFrameToBuffer(stream, len);
}
