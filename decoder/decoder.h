#ifndef DECODER_H
#define DECODER_H

#include <QObject>
#include <QReadWriteLock>
#include <atomic>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
}

class Decoder : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Decoder)

public:
    explicit Decoder(QObject *parent = nullptr);
    ~Decoder();

    int init(AVCodecParameters *codecpar, AVMediaType type, const QString &decodeName = "");
    int initHardware(AVHWDeviceType hwType = AV_HWDEVICE_TYPE_CUDA);
    int decode(AVPacket *pkt, AVFrame *&outFrame);
    void useHardware(bool isUse);
    AVHWDeviceType getBestHardwareType();
    QString getHardwareDecoderName(AVCodecID codecId, AVHWDeviceType hwType);

    int flush();
    void close();

    AVCodecContext* codecCtx() const;
    AVMediaType mediaType() const;
    bool isHardware() const;
    bool isOpen() const;

private:
    // 内部工具
    AVCodec* findDecoder(AVCodecParameters *codecpar, AVHWDeviceType hwType,const QString &decodeName = "");
    int hwFrameTransfer(AVFrame *srcFrame, AVFrame *&dstFrame);
    void closeInternal();

    // 硬解标准回调
    static enum AVPixelFormat hwPixFmtCallback(AVCodecContext *ctx, const enum AVPixelFormat *pixFmts);

private:
    AVCodecContext *codecCtx_ = nullptr;
    AVMediaType mediaType_ = AVMEDIA_TYPE_UNKNOWN;
    AVBufferRef *hwDeviceCtx_ = nullptr;
    AVFrame *hwTmpFrame_ = nullptr;  // 硬解中转帧

    std::atomic<bool> isOpened_{false};
    std::atomic<bool> useHardware_{false};
    mutable QReadWriteLock lock_;
};

#endif // DECODER_H
