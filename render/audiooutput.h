#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#ifdef __cplusplus
extern "C"
{
#include <SDL2/SDL.h>
#include "libavutil/avutil.h"
#include "libavutil/samplefmt.h"
}
#endif

#include "queue/avframequeue.h"
#include "resampler/resampler.h"
#include <cstdint>
#include <QMutex>

extern"C"{
#include <libavformat/avformat.h>
}
class AVSyncClock;
class AudioOutput
{
public:
    AudioOutput(const Resampler::AudioSpec &inSpec,
                const Resampler::AudioSpec &outSpec,
                AVFrameQueue *frameQueue,
                AVSyncClock* sync_clock);

    ~AudioOutput();

    int Init();
    int DeInit();

    void pause();
    void resume();

    void setAudioTimebase(AVRational tb);
    void setVolume(int volume); // volume: 0~100
    void setMute(bool mute);
    bool isMute() const;
    int getVolume() const;

    int64_t getAudioClock() const;
    bool isAudioInitialized() const { return is_audio_init_; }

private:
    // SDL音频回调
    static void fill_audio_pcm(void *udata, Uint8 *stream, int len);
    // SDL回调
    int resampleFrameToBuffer(uint8_t* stream, int len);
    void applyVolume(uint8_t* data, int len);

private:
    Resampler*   resampler_ = nullptr;
    AVSyncClock* sync_clock_ = nullptr;
    Resampler::AudioSpec in_spec_;
    Resampler::AudioSpec out_spec_;
    AVFrameQueue* frame_queue_ = nullptr;

    uint8_t* audio_buf_ = nullptr;
    uint32_t audio_buf_size_ = 0;
    uint32_t audio_buf_index_ = 0;

    FILE* dump_pcm_ = nullptr;
    bool is_audio_init_ = false;
    mutable QMutex mutex_;  // 线程锁

    int64_t audio_clock_us_ = 0;
    int sample_rate_ = 0;
    bool need_resample_ = false;

    int volume_ = 50;    // 默认音量 50%
    bool mute_ = false;   // 默认非静音
    AVRational audio_timebase_;
};

#endif // AUDIOOUTPUT_H
