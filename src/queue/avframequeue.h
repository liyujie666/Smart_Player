#ifndef AVFRAMEQUEUE_H
#define AVFRAMEQUEUE_H


#include "avqueue.h"

#ifdef __cplusplus
extern "C"
{
#include "libavcodec/avcodec.h"
}
#endif
class AVFrameQueue
{
public:
    AVFrameQueue();
    ~AVFrameQueue();
    void Abort();
    int Push(AVFrame *val);
    AVFrame *Pop(const int timeout = 0);
    AVFrame *Front();
    int Size();
    bool isEmpty();
    void clear();
    void dropFront();
private:
    void release();
    AVQueue<AVFrame *> queue_;
};


#endif // AVFRAMEQUEUE_H
