#ifndef PREVIEWTHREAD_H
#define PREVIEWTHREAD_H
#include <QThread>
#include <QImage>
#include <atomic>
#include <mutex>
#include "videoplayer.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext.h>
}
class PreviewThread : public QThread
{
    Q_OBJECT
public:
    PreviewThread(VideoPlayer* player);
    ~PreviewThread();

    void setTargetPts(int64_t pts, int index);  // 请求预览某个时间点
    void setUseHwAccel(bool enabled);           // 设置软硬解切换
    void stop();                                // 退出线程

signals:
    void previewReady(int index, QImage image); // 返回预览图

protected:
    void run() override;

private:
    void initDecoder();                         // 初始化预览解码器
    void releaseDecoder();                      // 清理解码资源
    void decodePreviewFrame();


private:
    AVFormatContext *preview_fmtCtx_ = nullptr;
    AVCodecContext *codecCtx_ = nullptr;
    AVStream *stream_ = nullptr;
    SwsContext *swsCtx_ = nullptr;
    AVFrame *frame_ = nullptr;
    AVFrame *outFrame_ = nullptr;
    AVFrame *swFrame_ = nullptr;
    AVBufferRef *hwDeviceCtx_ = nullptr;

    bool useHwAccel_ = false;

    std::atomic<int64_t> targetPts_{-1};
    std::atomic<int> currentIndex_ {-1};
    std::atomic<bool> running_{true};

    std::mutex mutex_;
    VideoPlayer *player_;
};

#endif // PREVIEWTHREAD_H
