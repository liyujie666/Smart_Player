#ifndef DEMUXER_H
#define DEMUXER_H

#include <QObject>
#include <QReadWriteLock>
#include <atomic>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

class Demuxer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Demuxer)

public:
    enum class MediaType {
        FILE_TYPE,
        RTSP_TYPE,
        RTMP_TYPE,
        HTTP_TYPE,    // 新增
        HTTPS_TYPE,   // 新增
        HLS_TYPE      // 新增
    };

    explicit Demuxer(QObject *parent = nullptr);
    ~Demuxer();

    int open(const char *filename);
    void close();
    int readPacket(AVPacket *pkt);
    int seek(int64_t timestamp, bool videoSeek = true);
    
    // 获取媒体流信息
    AVStream* getStream(AVMediaType type) const;
    int getStreamIndex(AVMediaType type) const;
    bool hasStream(AVMediaType type) const;

    
    // 媒体信息
    int64_t getDuration() const;
    MediaType mediaType() const;
    AVFormatContext* formatContext() const;
    bool isOpen() const;
    

private:
    static MediaType parseMediaType(const char *filename);
    void findStreams();
    void closeInternal();
    int getStreamIndexInternal(AVMediaType type) const;
    bool isAttachedPic(AVStream* stream);
    static int interruptCallback(void* opaque);


    AVFormatContext *fmtCtx_ = nullptr;
    MediaType mediaType_ = MediaType::FILE_TYPE;

    int audioStreamIndex_ = -1;
    int videoStreamIndex_ = -1;
    int coverStreamIndex_ = -1;

    mutable QReadWriteLock lock_;
    std::atomic<bool> abort_{false};
    std::atomic<bool> isOpened_{false};
    

};

#endif // DEMUXER_H
