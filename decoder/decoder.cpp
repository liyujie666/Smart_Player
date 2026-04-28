#include "decoder.h"
#include <QDebug>
#include <QString>

#define FF_CHECK(ret, func) \
if (ret < 0) { \
        char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0}; \
        av_strerror(ret, errBuf, sizeof(errBuf)); \
        qCritical() << "[" #func "] Error:" << errBuf << "Code:" << ret; \
}

enum AVPixelFormat Decoder::hwPixFmtCallback(AVCodecContext *ctx, const enum AVPixelFormat *pixFmts)
{
    AVHWDeviceContext *hwDevCtx = (AVHWDeviceContext*)ctx->hw_device_ctx->data;
    AVHWDeviceType hwType = hwDevCtx->type;

    for (int i = 0; pixFmts[i] != AV_PIX_FMT_NONE; i++) {
        switch(hwType) {
        case AV_HWDEVICE_TYPE_CUDA:    if (pixFmts[i] == AV_PIX_FMT_CUDA) return pixFmts[i]; break;
        case AV_HWDEVICE_TYPE_D3D11VA: if (pixFmts[i] == AV_PIX_FMT_D3D11) return pixFmts[i]; break;
        case AV_HWDEVICE_TYPE_QSV:     if (pixFmts[i] == AV_PIX_FMT_QSV) return pixFmts[i]; break;
        case AV_HWDEVICE_TYPE_VAAPI:   if (pixFmts[i] == AV_PIX_FMT_VAAPI) return pixFmts[i]; break;
        default: break;
        }
    }
    return AV_PIX_FMT_YUV420P;
}

Decoder::Decoder(QObject *parent) : QObject(parent) {
}

Decoder::~Decoder() { close(); }

int Decoder::init(AVCodecParameters *codecpar, AVMediaType type, const QString &decodeName)
{
    QWriteLocker locker(&lock_);
    closeInternal();

    if (!codecpar) return AVERROR(EINVAL);
    mediaType_ = type;

    // 确定硬件类型
    AVHWDeviceType hwType = AV_HWDEVICE_TYPE_NONE;
    if (useHardware_.load() && mediaType_ == AVMEDIA_TYPE_VIDEO) {
        hwType = getBestHardwareType();
    }
    // 查找解码器
    const AVCodec* codec = findDecoder(codecpar, hwType, decodeName);
    if (!codec) {
        qCritical() << "未找到合适的解码器";
        return AVERROR_DECODER_NOT_FOUND;
    }

    // 创建解码上下文
    codecCtx_ = avcodec_alloc_context3(codec);
    if (!codecCtx_) return AVERROR(ENOMEM);

    int ret = avcodec_parameters_to_context(codecCtx_, codecpar);
    if (ret < 0) goto failed;

    // 初始化硬件设备
    if (hwType != AV_HWDEVICE_TYPE_NONE) {
        ret = initHardware(hwType);
        if (ret < 0) {
            useHardware_.store(false);
            avcodec_free_context(&codecCtx_);
            return init(codecpar, type, decodeName);
        } else {
            codecCtx_->get_format = Decoder::hwPixFmtCallback;
            codecCtx_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
        }
    }

    // 打开解码器
    ret = avcodec_open2(codecCtx_, codec, nullptr);
    if (ret < 0) goto failed;

    isOpened_ = true;

    qDebug() << "解码器初始化成功 | 模式:" << (useHardware_.load() ? "硬件解码" : "软件解码")
             << "| 解码器名称:" << codec->name;
    return 0;

failed:
    closeInternal();
    return ret;
}

// 跨平台硬解初始化
int Decoder::initHardware(AVHWDeviceType hwType)
{
    if (hwDeviceCtx_) return 0;

    int ret = av_hwdevice_ctx_create(&hwDeviceCtx_, hwType, nullptr, nullptr, 0);
    FF_CHECK(ret, av_hwdevice_ctx_create);
    if (ret < 0) return ret;

    if (!hwTmpFrame_) {
        hwTmpFrame_ = av_frame_alloc();
        if (!hwTmpFrame_) return AVERROR(ENOMEM);
    }
    return 0;
}

int Decoder::decode(AVPacket *pkt, AVFrame *&outFrame)
{
    if (!isOpened_) return AVERROR(EINVAL);
    av_frame_unref(outFrame);

    QReadLocker locker(&lock_);
    // 发送包
    int ret = avcodec_send_packet(codecCtx_, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return ret;
    if (ret < 0) { FF_CHECK(ret, avcodec_send_packet); return ret; }

    // 接收帧
    ret = avcodec_receive_frame(codecCtx_, outFrame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return ret;
    if (ret < 0) { FF_CHECK(ret, avcodec_receive_frame); return ret; }

    // 硬解帧转CPU
    if (useHardware_ && mediaType_ == AVMEDIA_TYPE_VIDEO) {
        ret = hwFrameTransfer(outFrame, hwTmpFrame_);
        FF_CHECK(ret, hwFrameTransfer);
        if (ret >= 0) {
            av_frame_unref(outFrame);
            av_frame_move_ref(outFrame, hwTmpFrame_);
        } else {
            av_frame_unref(outFrame);
        }
    }

    return 0;
}

void Decoder::useHardware(bool isUse)
{
    useHardware_.store(isUse);
}

// 刷新解码器（seek）
int Decoder::flush()
{
    if (!isOpened_) return AVERROR(EINVAL);
    QWriteLocker locker(&lock_);
    avcodec_flush_buffers(codecCtx_);
    if (hwTmpFrame_) {
        av_frame_unref(hwTmpFrame_);
    }
    return 0;
}

void Decoder::close() {
    QWriteLocker locker(&lock_);
    closeInternal();
}

void Decoder::closeInternal()
{
    isOpened_ = false;

    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }
    if (hwDeviceCtx_) {
        av_buffer_unref(&hwDeviceCtx_);
        hwDeviceCtx_ = nullptr;
    }
    if (hwTmpFrame_) {
        av_frame_unref(hwTmpFrame_);
        av_frame_free(&hwTmpFrame_);
        hwTmpFrame_ = nullptr;
    }

    mediaType_ = AVMEDIA_TYPE_UNKNOWN;
}

// 硬解帧转CPU
int Decoder::hwFrameTransfer(AVFrame *srcFrame, AVFrame *&dstFrame)
{
    if (!srcFrame || !dstFrame) return AVERROR(EINVAL);
    av_frame_unref(dstFrame);

    // 从GPU显存转移到CPU内存
    int ret = av_hwframe_transfer_data(dstFrame, srcFrame, 0);
    FF_CHECK(ret, av_hwframe_transfer_data);
    if (ret < 0) return ret;

    // 复制宽高、时间戳等属性
    av_frame_copy_props(dstFrame, srcFrame);

    return 0;
}

// 查找解码器
AVCodec* Decoder::findDecoder(AVCodecParameters *codecpar, AVHWDeviceType hwType, const QString &decodeName)
{
    QString name = decodeName.trimmed();

    // 1. 用户指定解码器
    if (!name.isEmpty()) {
        const AVCodec* codec = avcodec_find_decoder_by_name(name.toStdString().c_str());
        if (codec) {
            // 硬解解码器直接可用（FFmpeg自动匹配编码）
            bool isHw = name.contains("_cuvid") || name.contains("_qsv") || name.contains("_d3d11va");
            if (isHw || codec->id == codecpar->codec_id) {
                return const_cast<AVCodec*>(codec);
            }
        }
        qDebug() << "指定解码器" << name << "不支持，自动使用默认解码器";
    }

    // 2. 自动硬件解码器
    if (hwType != AV_HWDEVICE_TYPE_NONE) {
        QString hwName = getHardwareDecoderName(codecpar->codec_id, hwType);
        if (!hwName.isEmpty()) {
            const AVCodec* codec = avcodec_find_decoder_by_name(hwName.toStdString().c_str());
            if (codec) return const_cast<AVCodec*>(codec);
        }
    }

    // 3. 默认软解
    return const_cast<AVCodec*>(avcodec_find_decoder(codecpar->codec_id));
}
AVHWDeviceType Decoder::getBestHardwareType()
{
    // NVIDIA CUDA > Intel QSV > Windows D3D11VA > Linux VAAPI
    static const AVHWDeviceType priority_list[] = {
        AV_HWDEVICE_TYPE_CUDA,
        AV_HWDEVICE_TYPE_QSV,
        AV_HWDEVICE_TYPE_D3D11VA,
        AV_HWDEVICE_TYPE_VAAPI,
        AV_HWDEVICE_TYPE_DXVA2,
        AV_HWDEVICE_TYPE_NONE
    };

    AVHWDeviceType current_type = AV_HWDEVICE_TYPE_NONE;
    while ((current_type = av_hwdevice_iterate_types(current_type)) != AV_HWDEVICE_TYPE_NONE)
    {
        for (int i = 0; priority_list[i] != AV_HWDEVICE_TYPE_NONE; i++)
        {
            if (current_type == priority_list[i])
            {
                return current_type;
            }
        }
    }

    return AV_HWDEVICE_TYPE_NONE;
}

QString Decoder::getHardwareDecoderName(AVCodecID codecId, AVHWDeviceType hwType)
{
    auto formatName = [](AVCodecID id) -> const char* {
        switch (id) {
        case AV_CODEC_ID_H264: return "h264";
        case AV_CODEC_ID_HEVC: return "hevc";
        case AV_CODEC_ID_VP9:  return "vp9";
        case AV_CODEC_ID_AV1:  return "av1";
        default: return nullptr;
        }
    };
    const char* codec = formatName(codecId);
    if (!codec) return "";

    switch (hwType) {
    case AV_HWDEVICE_TYPE_CUDA:    return QString("%1_cuvid").arg(codec);
    case AV_HWDEVICE_TYPE_QSV:     return QString("%1_qsv").arg(codec);
    case AV_HWDEVICE_TYPE_D3D11VA: return QString("%1_d3d11va").arg(codec);
    case AV_HWDEVICE_TYPE_VAAPI:   return QString("%1_vaapi").arg(codec);
    case AV_HWDEVICE_TYPE_DXVA2:   return QString("%1_dxva2").arg(codec);
    default: return "";
    }
}

AVCodecContext* Decoder::codecCtx() const { QReadLocker locker(&lock_); return codecCtx_; }
AVMediaType Decoder::mediaType() const { QReadLocker locker(&lock_); return mediaType_; }
bool Decoder::isHardware() const { return useHardware_; }
bool Decoder::isOpen() const { return isOpened_; }
