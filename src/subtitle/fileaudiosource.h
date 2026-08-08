#ifndef FILEAUDIOSOURCE_H
#define FILEAUDIOSOURCE_H

#include "iaudiosource.h"
#include "demuxer/demuxer.h"
#include "decoder/decoder.h"
#include "resampler/resampler.h"

#include <QString>
#include <memory>
#include <atomic>
#include <vector>

/**
 * @brief 文件音频源 —— Pull 模式
 *
 * 负责：打开本地文件 → Demux → Decode → Resample → 产出 16kHz/mono/float32 PCM
 * 不关心识别逻辑，仅作为 AsrPipeline 的数据供给者。
 */
class FileAudioSource : public IAudioSource {
public:
    explicit FileAudioSource(const QString& url);
  ~FileAudioSource() override;

    AudioSourceMode mode() const override { return AudioSourceMode::Pull; }

    bool open() override;
    void close() override;

    int pull(float* out, int maxSamples, double& mediaTimeSec) override;

    bool isEof() const override { return eof_; }
    void cancel() override { cancelled_ = true; }
    bool isCancelled() const override { return cancelled_; }

    /// Seek 到指定位置（秒）：seek 内部 demuxer、flush decoder、清空 PCM 缓冲
    bool seekTo(double pos_sec) override;

    /// 获取音频流（供 Pipeline 初始化 resampler 规格参考，如不需要可忽略）
    AVStream* audioStream() const;

private:
    /// 从 demuxer 读取并解码，填充内部 PCM 缓冲
  bool fillBuffer();

private:
    QString url_;
    std::unique_ptr<Demuxer> demux_;
    std::unique_ptr<Decoder> dec_;
    std::unique_ptr<Resampler> res_;

    int audio_stream_idx_ = -1;
    AVRational audio_tb_{0, 0};  // 音频流时间基，用于 pts→秒换算
    double current_time_sec_ = 0.0;
    bool time_calibrated_ = false;  // seek 后是否已用 pts 校准时间

    // 内部 PCM 缓冲（解码+重采样后暂存，供 pull 使用）
    std::vector<float> pcm_buf_;
    size_t pcm_read_pos_ = 0;

    std::atomic<bool> cancelled_{false};
    std::atomic<bool> eof_{false};
};

#endif // FILEAUDIOSOURCE_H
