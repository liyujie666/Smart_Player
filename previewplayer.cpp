#include "previewplayer.h"
#include <QDebug>

PreviewPlayer::PreviewPlayer(QObject *parent) : QObject(parent)
{
    workThread_ = new QThread(this);
    this->moveToThread(workThread_);
    workThread_->start();
}

PreviewPlayer::~PreviewPlayer()
{
    stop();
    workThread_->quit();
    workThread_->wait();
    delete workThread_;
}

bool PreviewPlayer::open(const QString &filePath)
{
    // 🔥 open在主线程调用，简单判断，无锁竞争
    if (filePath == currentFile_ && demuxer_ && decoder_)
        return true;

    release();
    currentFile_ = filePath;

    demuxer_ = new Demuxer();
    if (demuxer_->open(filePath.toStdString().c_str()) < 0) {
        release();
        return false;
    }
    if (!demuxer_->hasStream(AVMEDIA_TYPE_VIDEO)) {
        release();
        return false;
    }

    decoder_ = new Decoder();
    AVStream* vs = demuxer_->getStream(AVMEDIA_TYPE_VIDEO);
    if (decoder_->init(vs->codecpar, AVMEDIA_TYPE_VIDEO) < 0) {
        release();
        return false;
    }
    return true;
}

void PreviewPlayer::requestPreview(int64_t seekTimeSec)
{
    QMutexLocker lock(&request_mutex_);
    latest_seek_time_ = seekTimeSec;

    QMetaObject::invokeMethod(this, [this, seekTimeSec]() {
        this->decode(seekTimeSec);
    }, Qt::QueuedConnection);
}

void PreviewPlayer::decode(int64_t seekTimeSec)
{

    {
        QMutexLocker lock(&request_mutex_);
        if (seekTimeSec != latest_seek_time_) {
            return;
        }
    }

    if (!demuxer_ || !decoder_ || currentFile_.isEmpty())
        return;

    AVFrame*  frame   = av_frame_alloc();
    AVPacket* pkt     = av_packet_alloc();
    if (!frame || !pkt) {
        qCritical() << "PreviewPlayer: FFmpeg allocation failed";
        if (frame) av_frame_free(&frame);
        if (pkt) av_packet_free(&pkt);
        return;
    }
    QByteArray outData;
    int outW = 0, outH = 0;
    AVPixelFormat outFmt = AV_PIX_FMT_NONE;

    // 关键帧Seek
    int64_t seek_ts = seekTimeSec * 1000000;
    av_seek_frame(demuxer_->formatContext(), -1, seek_ts, AVSEEK_FLAG_BACKWARD);
    decoder_->flush();


    while (av_read_frame(demuxer_->formatContext(), pkt) == 0)
    {
        if (pkt->stream_index != demuxer_->getStreamIndex(AVMEDIA_TYPE_VIDEO)) {
            av_packet_unref(pkt);
            continue;
        }

        if (decoder_->decode(pkt, frame) == 0) {
            outW = frame->width;
            outH = frame->height;
            outFmt = (AVPixelFormat)frame->format;

            if (outFmt == AV_PIX_FMT_YUV420P) {
                int y = outW * outH;
                outData.append((char*)frame->data[0], y);
                outData.append((char*)frame->data[1], y/4);
                outData.append((char*)frame->data[2], y/4);
            }
            else if (outFmt == AV_PIX_FMT_NV12) {
                outData.append((char*)frame->data[0], outW * outH);
                outData.append((char*)frame->data[1], outW * outH / 2);
            }
            break;
        }
        av_packet_unref(pkt);
    }

    {
        QMutexLocker lock(&request_mutex_);
        if (seekTimeSec != latest_seek_time_ || outData.isEmpty()) {
            av_packet_free(&pkt);
            av_frame_free(&frame);
            return;
        }
    }

    // 发送结果
    emit previewFrameReady(outData, outW, outH, outFmt);

    av_packet_free(&pkt);
    av_frame_free(&frame);
}

void PreviewPlayer::stop()
{
    release();
    currentFile_.clear();
    // 重置请求
    QMutexLocker lock(&request_mutex_);
    latest_seek_time_ = -1;
}

void PreviewPlayer::release()
{
    if (decoder_) {
        decoder_->close();
        delete decoder_;
        decoder_ = nullptr;
    }
    if (demuxer_) {
        demuxer_->close();
        delete demuxer_;
        demuxer_ = nullptr;
    }
}
