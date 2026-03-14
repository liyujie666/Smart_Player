#include "picturecreator.h"
#include <QDebug>


PictureCreator::PictureCreator() {
    avformat_network_init();
}

PictureCreator::~PictureCreator() {
    avformat_network_deinit();
}

QImage PictureCreator::getPreViewImage(const QString &videoPath, int maxWidth, int maxHeight)
{
    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    AVCodecParameters *codecParams = nullptr;
    const AVCodec *codec = nullptr;
    int videoStreamIdx = -1;
    AVFrame *frame = av_frame_alloc();
    QImage previewImage;

    QString type = getFileType(videoPath);
    if(type == "FILE")
    {
        // 1. 打开视频文件
        if (avformat_open_input(&fmtCtx, videoPath.toStdString().c_str(), nullptr, nullptr) != 0) {
            qWarning() << "Failed to open video file:" << videoPath.toStdString().c_str();
            goto cleanup;
        }

        // 2. 查找流信息
        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
            qWarning() << "Failed to find stream info";
            goto cleanup;
        }

        // 3. 查找视频流
        duration_ = fmtCtx->duration * av_q2d(AV_TIME_BASE_Q);

        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIdx = i;
                break;
            }
        }
        if (videoStreamIdx == -1) {
            qWarning() << "No video stream found";
            goto cleanup;
        }

        // 4. 初始化解码器
        codecParams = fmtCtx->streams[videoStreamIdx]->codecpar;
        codec = avcodec_find_decoder(codecParams->codec_id);
        if (!codec) {
            qWarning() << "Unsupported codec";
            goto cleanup;
        }

        codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecCtx, codecParams);
        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            qWarning() << "Failed to open codec";
            goto cleanup;
        }

        // 5. 读取并解码第一帧
        AVPacket packet;
        while (av_read_frame(fmtCtx, &packet) >= 0) {
            if (packet.stream_index == videoStreamIdx) {
                if (avcodec_send_packet(codecCtx, &packet) == 0) {
                    if (avcodec_receive_frame(codecCtx, frame) == 0) {
                        // 成功解码到帧，转换为 QImage
                        previewImage = convertFrameToQImage(frame, maxWidth, maxHeight);
                        break;
                    }
                }
            }
            av_packet_unref(&packet);
        }
    }else if(type == "RTSP"){
        duration_ = 0;
        return QImage(":/SmartPlayer-icon/image_rtsp.png");
    }else if(type == "RTMP"){
        duration_ = 0;
        return QImage(":/SmartPlayer-icon/image_rtmp.png");
    }



cleanup:
    // 释放资源
    if (codecCtx) avcodec_free_context(&codecCtx);
    if (fmtCtx) avformat_close_input(&fmtCtx);
    if (frame) av_frame_free(&frame);

    return previewImage;
}

QImage PictureCreator::convertFrameToQImage(AVFrame *frame, int maxWidth, int maxHeight)
{
    SwsContext *swsCtx = sws_getContext(
        frame->width, frame->height, (AVPixelFormat)frame->format,
        maxWidth, maxHeight, AV_PIX_FMT_RGB24, // 输出为 RGB24 格式
        SWS_BILINEAR, nullptr, nullptr, nullptr
        );

    if (!swsCtx) return QImage();

    // 创建目标 AVFrame
    AVFrame *rgbFrame = av_frame_alloc();
    rgbFrame->format = AV_PIX_FMT_RGB24;
    rgbFrame->width = maxWidth;
    rgbFrame->height = maxHeight;
    av_frame_get_buffer(rgbFrame, 0);

    // 转换像素格式并缩放
    sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height,
              rgbFrame->data, rgbFrame->linesize);

    // 创建 QImage
    QImage image(
        rgbFrame->data[0],
        maxWidth,
        maxHeight,
        rgbFrame->linesize[0], // 关键修复：使用实际行跨度
        QImage::Format_RGB888
        );

    // 深拷贝数据（避免 AVFrame 释放后数据丢失）
    QImage copy = image.copy();

    // 清理
    sws_freeContext(swsCtx);
    av_frame_free(&rgbFrame);

    return copy;
}

int PictureCreator::getDuration()
{
    return duration_;
}

QString PictureCreator::getFileType(const QString &videoPath)
{
    if(videoPath.toLower().endsWith(".mp4") || videoPath.toLower().endsWith(".mkv") || videoPath.toLower().endsWith(".avi"))
    {
        return "FILE";
    }else if(videoPath.startsWith("rtsp")){
        return "RTSP";
    }else if(videoPath.startsWith("rtmp")){
        return "RTMP";
    }
}
