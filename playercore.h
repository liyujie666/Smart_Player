#ifndef PLAYERCORE_H
#define PLAYERCORE_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <QQueue>
#include <QFuture>
#include "demuxer/demuxer.h"
#include "decoder/decoder.h"
#include "converter/videoconverter.h"
#include "resampler/resampler.h"
#include "filter/audiofilter.h"
#include "render/audiooutput.h"
#include "queue/avpacketqueue.h"
#include "queue/avframequeue.h"
#include "syncclock.h"

class PlayerCore : public QObject
{
    Q_OBJECT
public:
    // 播放器状态
    enum State {
        Stopped = 0,
        Running = 1,
        Paused = 2
    };

    explicit PlayerCore(QObject *parent = nullptr);
    ~PlayerCore();

    // 控制函数
    void open(const QString &url);
    void play();
    void pause();
    void stop();
    void setSpeed(int speedIndex);        // 倍速(1=0.5,2=1.0,3=1.5,4=2.0)

    // 硬件解码
    void useHardware(bool isUse);
    void setDecodeType(const QString& decoder);
    // seek
    void seek(int64_t pos_ms);
    // 截图
    void setScreenshotSavePath(const QString& savePath);
    void takeScreenshot();

    // 音量
    void setVolume(int val);
    void setMute(bool mute);
    bool isMute() const;

    // 获取信息
    int64_t getDuration() const;          // 总时长(ms)
    int64_t getCurrentPos() const;        // 当前播放位置(ms)
    State getState() const;               // 获取状态
    Demuxer::MediaType getMediaType() const;
    AVFormatContext* getAVFormatContext() const;
    QString getFileUrl() const;
    bool hasAudio() const;
    bool hasVideo() const;
signals:
    void stateChanged();
    void timeChanged();
    void initFinished();
    void playFailed(const QString& info);
    void playFinished();
    void openResult(bool result);
    void screecshotStatus(const QString& filePath,bool isOk);
    void frameYuv420pDecoded(const QByteArray& yuvData,int width,int height);
    void frameNv12Decoded(const QByteArray& yuvData,int width,int height);
    void frameRGBADecoded(const QByteArray& rgbData,int width,int height);
private:
    bool openInternal(const QString &url);
    void demuxThreadFunc();       // 解复用线程
    void audioDecodeThreadFunc(); // 音频解码线程
    void videoDecodeThreadFunc(); // 视频解码线程
    void videoRenderThreadFunc(); // 视频渲染线程

    void releaseResources();      // 释放所有资源
    void clearAllQueues();        // 清空所有队列(Seek/Stop用)
    void initAudioModule();       // 初始化音频模块(重采样/滤镜/输出)
    void initVideoModule();       // 初始化视频模块(转换器)
    void saveFrameToImage(const QByteArray& frame_data, int width, int height, AVPixelFormat format);
    double getSpeedFromIndex(int speedIndex);
private:

    // 状态控制
    std::atomic<State> state_;
    std::atomic<bool> is_exit_;       // 线程退出标志
    std::atomic<bool> is_seek_;       // Seek标志
    std::atomic<bool> need_screenshot_{false};
    QMutex mutex_;
    QWaitCondition cond_;

    // 核心模块
    Demuxer* demuxer_ = nullptr;
    Decoder* audio_decoder_ = nullptr;
    Decoder* video_decoder_ = nullptr;
    VideoConverter* converter_ = nullptr;
    Resampler* resampler_ = nullptr;
    AudioFilter* audio_filter_ = nullptr;
    AudioOutput* audio_output_ = nullptr;

    // 队列（解复用→解码→播放）
    AVPacketQueue* audio_pkt_queue_ = nullptr;
    AVPacketQueue* video_pkt_queue_ = nullptr;
    AVFrameQueue* audio_frame_queue_ = nullptr;
    AVFrameQueue* video_frame_queue_ = nullptr;

    static constexpr int MAX_AUDIO_PKT = 5;
    static constexpr int MAX_VIDEO_PKT = 10;
    static constexpr int MAX_AUDIO_FRAME = 8;
    static constexpr int MAX_VIDEO_FRAME = 15;
    // 线程
    QThread* demux_thread_ = nullptr;
    QThread* audio_decode_thread_ = nullptr;
    QThread* video_decode_thread_ = nullptr;
    QThread* video_render_thread_ = nullptr;
    AVSyncClock* sync_clock_ = nullptr;

    // 媒体参数
    QString file_url_ = nullptr;
    int64_t duration_ms_ = 0;
    int video_stream_idx_ = -1;
    int audio_stream_idx_ = -1;
    bool hardware_enabled_ = false;
    QString decoder_type_ = "";
    QString screenshot_save_path_ = "";
    bool hasAudio_ = false;
    bool hasVideo_ = false;

};

#endif // PLAYERCORE_H
