#include "videoconverter.h"
#include <QDebug>

#define FF_CHECK(ret, func) \
if (ret < 0) { \
        char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0}; \
        av_strerror(ret, errBuf, sizeof(errBuf)); \
        qCritical() << "[" #func "] Error:" << errBuf; \
}

VideoConverter::VideoConverter(QObject *parent) : QObject(parent) {}

VideoConverter::~VideoConverter() {
    close();
}

int VideoConverter::init(const VideoSpec &inSpec, const VideoSpec &outSpec)
{
    QWriteLocker locker(&lock_);
    closeInternal();

    if (inSpec.width <= 0 || inSpec.height <= 0 || outSpec.width <= 0 || outSpec.height <= 0) {
        qCritical() << "Invalid width/height";
        return AVERROR(EINVAL);
    }
    if (inSpec.pixFmt == AV_PIX_FMT_NONE || outSpec.pixFmt == AV_PIX_FMT_NONE) {
        qCritical() << "Invalid pixel format";
        return AVERROR(EINVAL);
    }

    inSpec_ = inSpec;
    outSpec_ = outSpec;
    inSpec_.bufferSize = calcBufferSize(inSpec_);
    outSpec_.bufferSize = calcBufferSize(outSpec_);

    // 创建SWS上下文
    swsCtx_ = sws_getContext(
        inSpec_.width, inSpec_.height, inSpec_.pixFmt,
        outSpec_.width, outSpec_.height, outSpec_.pixFmt,
        SWS_BILINEAR | SWS_FULL_CHR_H_INT,  // 通用高质量算法
        nullptr, nullptr, nullptr
        );

    if (!swsCtx_) {
        qCritical() << "sws_getContext failed";
        return AVERROR(ENOMEM);
    }


    isReady_ = true;
    const char *inFmtName = av_get_pix_fmt_name(inSpec_.pixFmt);
    const char *outFmtName = av_get_pix_fmt_name(outSpec_.pixFmt);
    if (!inFmtName) inFmtName = "unknown";
    if (!outFmtName) outFmtName = "unknown";

    qDebug() << "VideoConverter init success:"
             << inFmtName << inSpec_.width << "x" << inSpec_.height
             << "->" << outFmtName << outSpec_.width << "x" << outSpec_.height;
    return 0;
}

int VideoConverter::convert(const AVFrame *inFrame, AVFrame *&outFrame)
{

    if (!isReady_ || !inFrame || !outFrame) {
        return AVERROR(EINVAL);
    }

    if (outFrame->width != outSpec_.width || outFrame->height != outSpec_.height ||
        outFrame->format != outSpec_.pixFmt)
    {
        av_frame_unref(outFrame);
        outFrame->width  = outSpec_.width;
        outFrame->height = outSpec_.height;
        outFrame->format = outSpec_.pixFmt;
        int ret = av_frame_get_buffer(outFrame, 1);
        if (ret < 0) {
            qCritical() << "Failed to allocate buffer for outFrame:" << ret;
            return ret;
        }
    }

    int ret = sws_scale(
        swsCtx_,
        inFrame->data,
        inFrame->linesize,
        0, inFrame->height,
        outFrame->data,
        outFrame->linesize
        );

    if (ret <= 0) {
        FF_CHECK(ret, sws_scale);
        return AVERROR_EXTERNAL;
    }

    av_frame_copy_props(outFrame, inFrame);

    return 0;
}

void VideoConverter::close() {
    QWriteLocker locker(&lock_);
    closeInternal();
}

void VideoConverter::closeInternal() {
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    isReady_ = false;
    inSpec_ = {};
    outSpec_ = {};
}

// 计算缓冲区大小
int VideoConverter::calcBufferSize(const VideoSpec &spec) {
    return av_image_get_buffer_size(spec.pixFmt, spec.width, spec.height, 1);
}


VideoConverter::VideoSpec VideoConverter::inSpec() const {
    QReadLocker locker(&lock_);
    return inSpec_;
}

VideoConverter::VideoSpec VideoConverter::outSpec() const {
    QReadLocker locker(&lock_);
    return outSpec_;
}

bool VideoConverter::isReady() const {
    return isReady_;
}
