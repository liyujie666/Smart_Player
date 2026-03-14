#include "videoplayer.h"
#include <QDebug>
#include <thread>
#include <QImage>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>

int VideoPlayer::initVideoInfo()
{
    //初始化视频解码器
    int ret = initDecoder(&vCodecCtx_,&vStream_,AVMEDIA_TYPE_VIDEO);
    RET(initDecoder);

    //设置视频帧率
    frameRate_ = vStream_->avg_frame_rate.num / vStream_->avg_frame_rate.den;

    frameDurationMs_ = static_cast<int>(1.0 / frameRate_ * 1000);

    //初始化像素格式转换
    ret = initSws();
    RET(initSws);

    return 0;
}

int VideoPlayer::initSws(){
    //输出frame的参数
    vSwsOutSpec_.height = vCodecCtx_->height >> 4 << 4; //保证是16的倍数
    vSwsOutSpec_.width = vCodecCtx_->width >> 4 << 4;
    vSwsOutSpec_.pixFmt = AV_PIX_FMT_RGB24;
    vSwsOutSpec_.size = av_image_get_buffer_size(vSwsOutSpec_.pixFmt,vSwsOutSpec_.width,vSwsOutSpec_.height,1);

    AVPixelFormat srcFmt;
    if (useHwAccel_ && hw_pix_fmt_ != AV_PIX_FMT_NONE) {
        srcFmt = AV_PIX_FMT_NV12;  // CUDA 解码输出的格式通常是 NV12
    } else {
        srcFmt = vCodecCtx_->pix_fmt;
    }

    //初始化像素格式转换的上下文
    vSwsCtx_ = sws_getContext(vCodecCtx_->width,vCodecCtx_->height,srcFmt,
                             vSwsOutSpec_.width,vSwsOutSpec_.height,vSwsOutSpec_.pixFmt,
                             SWS_BILINEAR,nullptr,nullptr,nullptr);
    if(!vSwsCtx_){
        qDebug() << "sws_getContext failed!";
        return -1;
    }

    //初始化像素格式转换的输入frame
    vSwsInFrame_ = av_frame_alloc();
    if(!vSwsInFrame_){
        qDebug() << "av_frame_alloc failed!";
        return -1;
    }

    // 初始化像素格式转换的输出frame
    vSwsOutFrame_ = av_frame_alloc();
    if(!vSwsOutFrame_){
        qDebug() << "av_frame_alloc failed!";
        return -1;
    }

    //分配vSwsOutFrame中data控件
    int ret = av_image_alloc(vSwsOutFrame_->data,
                             vSwsOutFrame_->linesize,
                             vSwsOutSpec_.width,
                             vSwsOutSpec_.height,
                             vSwsOutSpec_.pixFmt,
                             1);
    RET(av_image_alloc);

    return 0;


}

void VideoPlayer::addVideoPkt(AVPacket *pkt)
{
    vMutex_.lock();
    AVPacket *temp_pkt = av_packet_alloc();
    av_packet_move_ref(temp_pkt,pkt);
    vPktList_.push_back(temp_pkt);
    vMutex_.signal();
    vMutex_.unlock();
}

void VideoPlayer::clearVideoPktList()
{
    vMutex_.lock();
    for(AVPacket *pkt : vPktList_){
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }
    vPktList_.clear();
    vMutex_.unlock();
}

void VideoPlayer::freeVideo(){
    clearVideoPktList();
    if(vCodecCtx_){
        avcodec_flush_buffers(vCodecCtx_);
        avcodec_free_context(&vCodecCtx_);
        vCodecCtx_ = nullptr;
    }
    if (vSwsCtx_) {
        sws_freeContext(vSwsCtx_);
        vSwsCtx_ = nullptr;
    }
    if(vSwsInFrame_){
        av_frame_free(&vSwsInFrame_);
        vSwsInFrame_ = nullptr;
    }
    if(swFrame_){
       av_frame_free(&swFrame_);
        swFrame_ = nullptr;
    }

    if(vSwsOutFrame_){
        av_frame_free(&vSwsOutFrame_);
    }
    if(hwDeviceCtx_){
        av_buffer_unref(&hwDeviceCtx_);
        hwDeviceCtx_ = nullptr;
    }

    vTime_ = 0;
    vStream_ = nullptr;
    vCanFree_ = false;
    vSeekTime_ = -1;
    hasVideo_ = false;
    isReady.store(false);

    qDebug() << "视频资源已被释放";
}

// void VideoPlayer::decodeVideoNoAudio()
// {
//     // 缓存队列
//     AVPacket* pkt = av_packet_alloc();
//     while (true) {
//         {
//             if(state_ == Stopped){
//                 vCanFree_ = true;
//                 // fmtCtxCanFree_ = true;
//                 break;
//             }
//             // if(state_ == Paused){
//             //     continue;
//             // }

//         }
//         if (av_read_frame(fmtCtx_, pkt) < 0) break;
//         if (pkt->stream_index == vStream_->index) {
//             if (avcodec_send_packet(vCodecCtx_, pkt) == 0) {
//                 while (avcodec_receive_frame(vCodecCtx_, vSwsInFrame_) == 0) {
//                     {
//                         if(state_ == Stopped){
//                             vCanFree_ = true;
//                             // fmtCtxCanFree_ = true;
//                             break;
//                         }
//                         // if(state_ == Paused){
//                         //     continue;
//                         // }
//                     }
//                     // 缓存视频帧

//                     sws_scale(vSwsCtx_, vSwsInFrame_->data, vSwsInFrame_->linesize, 0, vCodecCtx_->height,
//                               vSwsOutFrame_->data, vSwsOutFrame_->linesize);
//                     int outSize = vSwsOutSpec_.size;
//                     uint8_t *data = (uint8_t*)av_malloc(outSize);
//                     memcpy(data, vSwsOutFrame_->data[0], outSize);
//                     emit frameDecoded(this, data, vSwsOutSpec_);
//                     av_frame_unref(vSwsInFrame_);
//                     av_freep(data);


//                 }
//             }
//         }
//         av_packet_unref(pkt);
//     }

// }
void VideoPlayer::decodeVideoNoAudio() {
    while (true) {

        if (state_ == Stopped) {
            vCanFree_ = true;  // 通知可以释放视频相关资源
            break;
        }

        // 如果是暂停状态，且没有正在 seek，则不做解码
        if (state_ == Paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if(!isReady.load()){
            continue;
        }

        // 尝试从视频包队列取出一个 AVPacket
        vMutex_.lock();
        if (vPktList_.empty()) {
            vMutex_.unlock();
            // 队列为空，但解码器内部可能还存有未输出的帧
            receiveVideoFrames();
            // 等一会再取，避免空转
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // 取出队列头部的视频包
        AVPacket *pkt = vPktList_.front();
        vPktList_.pop_front();
        vMutex_.unlock();

        // 3) 计算并更新当前视频时钟 (vTime_)
        if (pkt->dts != AV_NOPTS_VALUE) {
            vTime_ = av_q2d(vStream_->time_base) * pkt->dts;
            emit timeChanged(this);
        }

        // 如果包无效，丢弃后继续
        if (!pkt->data || pkt->size <= 0) {
            printf("Skipping empty packet\n");
            av_packet_unref(pkt);
            continue;
        }
        // 发送该包给解码器   如果 avcodec_send_packet 返回 EAGAIN，说明解码器缓冲满了，需要先 receiveFrame 再继续发送
        while (true) {
            int ret = avcodec_send_packet(vCodecCtx_, pkt);
            if (ret == AVERROR(EAGAIN)) {
                // 解码器内部帧尚未读取完，先读取
                receiveVideoFrames();
                // 读完后，再尝试发送同一个 pkt
                continue;
            } else if (ret < 0) {  // 出现其它错误(例如流结束、解码器异常等)，丢弃该包
                av_packet_unref(pkt);
                break;
            } else {   // 发送成功
                av_packet_unref(pkt);
                receiveVideoFrames();   // 发送成功后，立即尝试把解码器内部的帧读出来
                break;
            }
        }
    }
    vCanFree_ = true;
}

void VideoPlayer::decodeVideo(bool isPreview)
{
    while (true) {

        if (state_ == Stopped) {
            vCanFree_ = true;  // 通知可以释放视频相关资源
            break;
        }

        // 如果是暂停状态，且没有正在 seek，则不做解码
        if (state_ == Paused && vSeekTime_ == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if(!isReady.load() && !isPreview){
            continue;
        }

        // 尝试从视频包队列取出一个 AVPacket
        vMutex_.lock();
        if (vPktList_.empty()) {
            vMutex_.unlock();
            // 队列为空，但解码器内部可能还存有未输出的帧
            receiveVideoFrames();
            // 等一会再取，避免空转
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // 取出队列头部的视频包
        AVPacket *pkt = vPktList_.front();
        vPktList_.pop_front();
        vMutex_.unlock();

        // 3) 计算并更新当前视频时钟 (vTime_)
        if (pkt->dts != AV_NOPTS_VALUE) {
            vTime_ = av_q2d(vStream_->time_base) * pkt->dts;
            emit timeChanged(this);
        }

        // 如果包无效，丢弃后继续
        if (!pkt->data || pkt->size <= 0) {
            printf("Skipping empty packet\n");
            av_packet_unref(pkt);
            continue;
        }
        // 发送该包给解码器   如果 avcodec_send_packet 返回 EAGAIN，说明解码器缓冲满了，需要先 receiveFrame 再继续发送
        while (true) {
            int ret = avcodec_send_packet(vCodecCtx_, pkt);
            if (ret == AVERROR(EAGAIN)) {
                // 解码器内部帧尚未读取完，先读取
                receiveVideoFrames();
                // 读完后，再尝试发送同一个 pkt
                continue;
            } else if (ret < 0) {  // 出现其它错误(例如流结束、解码器异常等)，丢弃该包
                av_packet_unref(pkt);
                break;
            } else {   // 发送成功
                av_packet_unref(pkt);
                receiveVideoFrames();   // 发送成功后，立即尝试把解码器内部的帧读出来
                break;
            }
        }
    }
    vCanFree_ = true;
}


void VideoPlayer::receiveVideoFrames()
{
    while (true) {
        // 1. 解码一帧
        int ret = avcodec_receive_frame(vCodecCtx_, vSwsInFrame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            qDebug() << "Error receiving video frame";
            break;
        }

        // 2. 判断是否硬解帧，如果是，转成可访问的CPU帧
        if(useHwAccel_){
            if (vSwsInFrame_->format == hw_pix_fmt_) {
                if (!swFrame_) {
                    swFrame_ = av_frame_alloc();
                }
                av_frame_unref(swFrame_);

                if (av_hwframe_transfer_data(swFrame_, vSwsInFrame_, 0) < 0) {
                    qDebug() << "av_hwframe_transfer_data failed";
                    av_frame_unref(swFrame_);
                    av_frame_free(&swFrame_);
                    av_frame_unref(vSwsInFrame_);
                    continue;
                }
                tmpFrame_ = swFrame_;
            }
        }else {
            tmpFrame_ = vSwsInFrame_;
        }

        // 4. 处理 seek
        if (vSeekTime_ >= 0) {
            if (vTime_ < vSeekTime_) {
                av_frame_unref(vSwsInFrame_);
                av_frame_unref(swFrame_);
                continue;
            } else {
                vSeekTime_ = -1;
            }
        }

        // 5. 像素格式转换
        ret = sws_scale(
            vSwsCtx_,
            tmpFrame_->data, tmpFrame_->linesize,
            0, vCodecCtx_->height,
            vSwsOutFrame_->data, vSwsOutFrame_->linesize
            );
        if (ret <= 0) {
            qDebug() << "sws_scale failed";
            av_frame_unref(vSwsInFrame_);
            av_frame_unref(swFrame_);
            continue;
        }

        // 6. 快照
        if (takeSnapshot_) {
            takeSnapshot_ = false;
            emit snapshotReady(vSwsOutFrame_, vSwsOutSpec_, snapshotPath_);
        }

        // 7. 音视频同步
        if (hasAudio_) {
            while (vTime_ > aTime_ && state_ == Playing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }else{
            std::this_thread::sleep_for(std::chrono::milliseconds(frameDurationMs_));
        }

        // 8. 发送渲染信号
        int outSize = vSwsOutSpec_.size;
        uint8_t *data = (uint8_t*)av_malloc(outSize);
        memcpy(data, vSwsOutFrame_->data[0], outSize);
        emit frameDecoded(this, data, vSwsOutSpec_);

        av_frame_unref(vSwsInFrame_);
        av_frame_unref(swFrame_);
    }
}

// void VideoPlayer::receiveVideoFrames()
// {
//     while (true) {
//         int ret = avcodec_receive_frame(vCodecCtx_, vSwsInFrame_);
//         if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {  // EAGAIN 表示当前帧取完了 / EOF 表示解码结束
//             break;
//         } else if (ret < 0) {   // 其它错误，直接跳过
//             break;
//         }
//         swFrame_ = av_frame_alloc();

//         if(vSwsInFrame_->format == hw_pix_fmt_){
//             if(av_hwframe_transfer_data(swFrame_,vSwsInFrame_,0) < 0){
//                 qDebug() << "av_hwframe_transfer_data failed";
//                 continue;
//             }
//             tmpFrame_ = swFrame_;
//         }else{
//             tmpFrame_ = vSwsInFrame_;
//         }

//         // 如果正在 seek，则判断是否需要丢弃早于 vSeekTime_ 的帧
//         if(vSeekTime_ >=0){
//             if(vTime_ < vSeekTime_){
//                 continue;
//             }else{
//                 vSeekTime_ = -1;
//             }
//         }

//         ret = sws_scale(
//             vSwsCtx_,
//             tmpFrame_->data, tmpFrame_->linesize,
//             0, vCodecCtx_->height,
//             vSwsOutFrame_->data, vSwsOutFrame_->linesize
//             );

//         if (ret <= 0) {  // 转换失败，丢弃该帧
//             continue;
//         }

//         if(takeSnapshot_){
//             takeSnapshot_ = false;
//             emit snapshotReady(vSwsOutFrame_,vSwsOutSpec_,snapshotPath_);
//             // saveFrameAsImage(vSwsOutFrame_,vSwsOutSpec_,snapshotPath_);
//         }


//         //如果有音频，需要等待音频同步
//         if (hasAudio_) {
//             while (vTime_ > aTime_ && state_ == Playing) {
//                 std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 适当休眠，让音频赶上视频
//             }
//         } else {
//             // 如果只有视频，可以按帧率做一个简单延时
//             // 例如：std::this_thread::sleep_for(std::chrono::milliseconds(40));
//         }

//         // 5) 拷贝像素数据给渲染层
//         //    注意，这里只是示例：把图像拷贝到 data 并通过信号/回调发送出去
//         int outSize = vSwsOutSpec_.size; // 例如 width*height*4(ARGB)
//         uint8_t *data = (uint8_t*)av_malloc(outSize);
//         memcpy(data, vSwsOutFrame_->data[0], outSize);
//         emit frameDecoded(this, data, vSwsOutSpec_);
//     }
// }


