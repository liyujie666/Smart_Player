#ifndef LIVEAUDIOSOURCE_H
#define LIVEAUDIOSOURCE_H

#include "iaudiosource.h"
#include "resampler/resampler.h"
#include "utils/audioringbuffer.h"

#include <memory>
#include <atomic>

/**
 * @brief 实时音频源 —— Push 模式
 *
 * 负责：接收外部送入的解码帧 → Resample → 写入 RingBuffer
 * Pipeline 通过 peek/consume 读取缓冲中的 PCM 数据。
 * 适用于 RTSP/RTMP 等流媒体场景。
 */
class LiveAudioSource : public IAudioSource {
public:
    /// @param audio 音频流（用于初始化 resampler 输入规格）
    explicit LiveAudioSource(AVStream* audio);
    ~LiveAudioSource() override;

    AudioSourceMode mode() const override { return AudioSourceMode::Push; }

    bool open() override;
    void close() override;

    // Push 模式接口
    void pushFrame(AVFrame* frame) override;
    size_t available() const override;
    void peek(float* out, size_t n) const override;
    double headTimeSec() const override;
    void consume(size_t n) override;

    bool isEof() const override { return cancelled_; }
    void cancel() override { cancelled_ = true; }
    bool isCancelled() const override { return cancelled_; }

    /// 清空缓冲区
    void clear();

private:
    AVStream* audio_stream_ = nullptr;  // 不拥有所有权
    AVRational tb_{0, 0};

    std::unique_ptr<Resampler> resampler_;
    AudioPcmRingBuffer ring_;

    std::atomic<bool> cancelled_{false};
};

#endif // LIVEAUDIOSOURCE_H
