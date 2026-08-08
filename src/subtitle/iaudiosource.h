#ifndef IAUDIOSOURCE_H
#define IAUDIOSOURCE_H

#include <vector>
#include <cstddef>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
}

// PCM 数据块：归一化 16kHz/mono/float32
struct PcmChunk {
    std::vector<float> samples;
    double media_time_sec = 0.0;  // 该 chunk 对应的媒体时间基准
};

// 音频源模式
enum class AudioSourceMode {
    Pull,   // 离线模式：主动拉取（FileAudioSource）
    Push    // 实时模式：外部推入帧，内部缓冲（LiveAudioSource）
};

/**
 * @brief 音频源纯虚接口
 *
 * 职责：只负责产出归一化 PCM（16kHz/mono/float32），
 * 把"音频从哪来"和"音频怎么识别"彻底解耦。
 *
 * Pull 模式（离线）：调用方循环调用 pull() 获取 PCM chunk
 * Push 模式（实时）：外部调用 pushFrame() 送入解码帧，
 *    调用方通过 peek/consume 读取缓冲数据
 */
class IAudioSource {
public:
    virtual ~IAudioSource() = default;

    /// 音频源模式
    virtual AudioSourceMode mode() const = 0;

    /// 打开音频源（初始化内部资源）
    virtual bool open() = 0;

  /// 关闭音频源（释放资源）
    virtual void close() = 0;

    // ===== Pull 模式接口（离线） =====

    /// 拉取一个 chunk（最大 maxSamples 个采样点）
    /// 返回实际采样点数，0 表示 EOF，-1 表示错误
 virtual int pull(float* out, int maxSamples, double& mediaTimeSec) {
        (void)out; (void)maxSamples; (void)mediaTimeSec;
  return -1;
    }

    // ===== Push 模式接口（实时） =====

    /// 外部推入解码后的音频帧（内部做重采样后写入缓冲）
    virtual void pushFrame(AVFrame* frame) { (void)frame; }

    /// 当前缓冲中可读的采样点数
    virtual size_t available() const { return 0; }

    /// 读取 n 个采样点（不消费）
    virtual void peek(float* out, size_t n) const { (void)out; (void)n; }

    /// 获取缓冲头部的媒体时间
virtual double headTimeSec() const { return 0.0; }

    /// 消费 n 个采样点（移动读指针）
    virtual void consume(size_t n) { (void)n; }

    // ===== 通用接口 =====

    /// 是否已到达末尾（离线=文件读完，实时=被取消）
    virtual bool isEof() const = 0;

    /// 协作式取消（stop/seek 时调用）
    virtual void cancel() = 0;

    /// 是否已被取消
    virtual bool isCancelled() const = 0;
};

#endif // IAUDIOSOURCE_H
