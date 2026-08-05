#include "demuxer.h"
#include <QDebug>
#include <QString>
#include <mutex>

#define FF_ERROR_BUF(ret) \
char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0}; \
    av_strerror(ret, errBuf, sizeof(errBuf));

#define CHECK_FF_ERROR(ret, func) \
if (ret < 0) { \
        FF_ERROR_BUF(ret); \
        qCritical() << "[" << #func << "] Failed:" << errBuf << "Code:" << ret; \
}

static void ffmpegGlobalInit() {
    static std::once_flag flag;
    std::call_once(flag, [](){
        avformat_network_init();  // 网络流必须初始化
        av_log_set_level(AV_LOG_ERROR); // 关闭冗余日志，提升性能
    });
}

Demuxer::Demuxer(QObject *parent) : QObject(parent)
{
    ffmpegGlobalInit();
}

Demuxer::~Demuxer()
{
    close();
}

int Demuxer::open(const char *filename)
{
    if (!filename) return AVERROR(EINVAL);

    QWriteLocker locker(&lock_);
    closeInternal();
    abort_ = false;
    ioActive_ = false;
    ioDeadlineUs_ = 0;
    
    // 分配上下文
    fmtCtx_ = avformat_alloc_context();
    if (!fmtCtx_) {
        qCritical() << "avformat_alloc_context failed";
        return AVERROR(ENOMEM);
    }

    // 设置中断回调（解决 RTSP/RTMP 卡死）
    fmtCtx_->interrupt_callback.callback = interruptCallback;
    fmtCtx_->interrupt_callback.opaque = this;

    // 解析媒体类型
    mediaType_ = parseMediaType(filename);

    // 网络流配置。应用层的 interrupt callback 仍是最终超时保障。
    AVDictionary* options = nullptr;
    if (mediaType_ == MediaType::RTSP_TYPE) {
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "timeout", "3000000", 0);
        av_dict_set(&options, "rw_timeout", "3000000", 0);
    } else if (mediaType_ == MediaType::RTMP_TYPE) {
        av_dict_set(&options, "rw_timeout", "3000000", 0);
    }

    ioActive_ = mediaType_ != MediaType::FILE_TYPE;
    ioDeadlineUs_ = ioActive_ ? av_gettime_relative() + OPEN_TIMEOUT_US : 0;
    int ret = avformat_open_input(&fmtCtx_, filename, nullptr, &options);
    ioActive_ = false;
    ioDeadlineUs_ = 0;
    av_dict_free(&options);
    CHECK_FF_ERROR(ret, avformat_open_input);
    if (ret < 0) {
        avformat_free_context(fmtCtx_);
        fmtCtx_ = nullptr;
        return ret;
    }

    // 读取流信息
    ioActive_ = mediaType_ != MediaType::FILE_TYPE;
    ioDeadlineUs_ = ioActive_ ? av_gettime_relative() + OPEN_TIMEOUT_US : 0;
    ret = avformat_find_stream_info(fmtCtx_, nullptr);
    ioActive_ = false;
    ioDeadlineUs_ = 0;
    CHECK_FF_ERROR(ret, avformat_find_stream_info);
    if (ret < 0) {
        closeInternal();
        return ret;
    }
    
    // 查找流信息
    findStreams();
    isOpened_ = true;

    qDebug() << "Demuxer open success! Video:" << videoStreamIndex_ << "Audio:" << audioStreamIndex_;
    return 0;
}

void Demuxer::close()
{
    requestAbort();
    QWriteLocker locker(&lock_);
    closeInternal();
}

void Demuxer::requestAbort()
{
    abort_.store(true, std::memory_order_release);
}

void Demuxer::resetAbort()
{
    abort_.store(false, std::memory_order_release);
}

void Demuxer::closeInternal()
{
    abort_ = true;
    ioActive_ = false;
    ioDeadlineUs_ = 0;
    isOpened_ = false;

    if (fmtCtx_) {
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
    }
    
    audioStreamIndex_ = -1;
    videoStreamIndex_ = -1;
}

int Demuxer::readPacket(AVPacket *pkt)
{
    if (!pkt || !isOpened_) return AVERROR(EINVAL);

    QReadLocker locker(&lock_);
    const bool networkStream = mediaType_ != MediaType::FILE_TYPE;
    ioActive_ = networkStream;
    ioDeadlineUs_ = networkStream ? av_gettime_relative() + READ_TIMEOUT_US : 0;
    int ret = av_read_frame(fmtCtx_, pkt);
    ioActive_ = false;
    ioDeadlineUs_ = 0;
    return ret;
}

int64_t Demuxer::timestampToUs(AVMediaType type, int64_t timestamp) const
{
    QReadLocker locker(&lock_);
    const int index = getStreamIndexInternal(type);
    if (!fmtCtx_ || index < 0 || timestamp == AV_NOPTS_VALUE) return AV_NOPTS_VALUE;
    return av_rescale_q(timestamp, fmtCtx_->streams[index]->time_base, AVRational{1, 1000000});
}

int Demuxer::seek(int64_t ts_us, bool useVideoStream)
{
    if (!isOpened_) return AVERROR(EINVAL);

    //QWriteLocker locker(&lock_);
    int streamIdx = -1;

    if (useVideoStream && videoStreamIndex_ >= 0) {
        streamIdx = videoStreamIndex_;
    } else if (audioStreamIndex_ >= 0) {
        streamIdx = audioStreamIndex_;
    } else {
        return AVERROR(EINVAL); // 无音视频流，失败
    }

    int64_t target_ts = av_rescale_q(
        ts_us,
        AV_TIME_BASE_Q,  // 输入时间基：1毫秒/单位
        fmtCtx_->streams[streamIdx]->time_base
        );

    int ret = av_seek_frame(fmtCtx_, streamIdx, target_ts, AVSEEK_FLAG_BACKWARD);
    CHECK_FF_ERROR(ret, av_seek_frame);
    return ret;
}

AVStream *Demuxer::getStream(AVMediaType type) const
{
    QReadLocker locker(&lock_);
    int idx = getStreamIndexInternal(type);
    return (idx >= 0 && fmtCtx_) ? fmtCtx_->streams[idx] : nullptr;
}

int Demuxer::getStreamIndex(AVMediaType type) const
{
    QReadLocker locker(&lock_);
    return getStreamIndexInternal(type);
}

bool Demuxer::hasStream(AVMediaType type) const
{
    QReadLocker locker(&lock_);
    return getStreamIndexInternal(type) >= 0;
}

int64_t Demuxer::getDuration() const
{
    QReadLocker locker(&lock_);
    if (!fmtCtx_ || fmtCtx_->duration <= 0) return 0.0;
    return (int64_t)fmtCtx_->duration * av_q2d(AV_TIME_BASE_Q);
}

Demuxer::MediaType Demuxer::mediaType() const
{
    QReadLocker locker(&lock_);
    return mediaType_;
}

AVFormatContext* Demuxer::formatContext() const
{
    QReadLocker locker(&lock_);
    return fmtCtx_;
}

bool Demuxer::isOpen() const
{
    return isOpened_;
}


int Demuxer::getStreamIndexInternal(AVMediaType type) const
{
    switch (type) {
    case AVMEDIA_TYPE_AUDIO: return audioStreamIndex_;
    case AVMEDIA_TYPE_VIDEO: return videoStreamIndex_;
    default: return -1;
    }
}

bool Demuxer::isAttachedPic(AVStream *stream)
{
    return stream->disposition & AV_DISPOSITION_ATTACHED_PIC;
}

void Demuxer::findStreams()
{
    audioStreamIndex_ = -1;
    videoStreamIndex_ = -1;
    coverStreamIndex_ = -1;

    for (int i = 0; i < fmtCtx_->nb_streams; i++) {
        AVStream* st = fmtCtx_->streams[i];
        AVCodecParameters* par = st->codecpar;
        if (!par) continue;

        // 专辑封面，不标记为视频流
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && isAttachedPic(st)) {
            coverStreamIndex_ = i;
            continue;
        }

        if (par->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex_ < 0) {
            videoStreamIndex_ = i;
        }

        if (par->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex_ < 0) {
            audioStreamIndex_ = i;
        }
    }
}

Demuxer::MediaType Demuxer::parseMediaType(const char *filename)
{
    QString url = QString(filename).toLower();
    if (url.startsWith("rtsp://")) return MediaType::RTSP_TYPE;
    if (url.startsWith("rtmp://")) return MediaType::RTMP_TYPE;
    return MediaType::FILE_TYPE;
}

int Demuxer::interruptCallback(void* opaque)
{
    auto* self = static_cast<Demuxer*>(opaque);
    if (self->abort_.load(std::memory_order_acquire)) return 1;
    if (!self->ioActive_.load(std::memory_order_acquire)) return 0;

    const int64_t deadline = self->ioDeadlineUs_.load(std::memory_order_acquire);
    return deadline > 0 && av_gettime_relative() >= deadline ? 1 : 0;
}
