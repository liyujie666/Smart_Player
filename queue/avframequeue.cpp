#include "avframequeue.h"
#include "utils/log.h"
AVFrameQueue::AVFrameQueue()
{

}

AVFrameQueue::~AVFrameQueue()
{
    Abort();
}

void AVFrameQueue::Abort()
{
    release();
    queue_.Abort();
}

int AVFrameQueue::Push(AVFrame *val)
{
    if (!val) return -1;
    AVFrame* tmp = av_frame_alloc();
    av_frame_move_ref(tmp,val);
    return queue_.Push(tmp);
}

AVFrame *AVFrameQueue::Pop(const int timeout)
{
    AVFrame *tmp_frame = NULL;
    int ret = queue_.Pop(tmp_frame, timeout);
    if (ret == -1) {
        LOG_ERROR("AVFrameQueue::Pop aborted");
    }
    return tmp_frame;
}

AVFrame *AVFrameQueue::Front()
{
    AVFrame *tmp_frame = NULL;
    int ret = queue_.Front(tmp_frame);
    if (ret == -1) {
        LOG_ERROR("AVFrameQueue::Front failed");
    }
    return tmp_frame;
}

int AVFrameQueue::Size()
{
    return queue_.Size();
}

bool AVFrameQueue::isEmpty()
{
    return queue_.isEmpty();
}

void AVFrameQueue::release()
{
    AVFrame *frame = NULL;
    while (true) {
        int ret = queue_.Pop(frame, 1);
        if(ret < 0) {
            break;
        } else {
            av_frame_free(&frame);
            continue;
        }
    }
}

void AVFrameQueue::clear() {
    release();
}

void AVFrameQueue::dropFront() {
    AVFrame *frame = Pop(0);
    if (frame) {
        av_frame_free(&frame);
    }
}
