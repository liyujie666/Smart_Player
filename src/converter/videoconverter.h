#ifndef VIDEOCONVERTER_H
#define VIDEOCONVERTER_H

#include <QObject>
#include <QReadWriteLock>
#include <atomic>

extern "C"
{
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

class VideoConverter : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(VideoConverter)

public:
    // 视频参数
    struct VideoSpec {
        int width = 0;
        int height = 0;
        AVPixelFormat pixFmt = AV_PIX_FMT_NONE;
        int bufferSize = 0;
    };

    explicit VideoConverter(QObject *parent = nullptr);
    ~VideoConverter();

    int init(const VideoSpec& inSpec, const VideoSpec& outSpec);
    int convert(const AVFrame *inFrame, AVFrame *&outFrame);
    void close();

    VideoSpec inSpec() const;
    VideoSpec outSpec() const;
    bool isReady() const;

private:
    void closeInternal();
    static int calcBufferSize(const VideoSpec& spec);

private:
    SwsContext* swsCtx_ = nullptr;
    VideoSpec inSpec_;
    VideoSpec outSpec_;

    mutable QReadWriteLock lock_;
    std::atomic<bool> isReady_{false};
};

#endif // VIDEOCONVERTER_H
