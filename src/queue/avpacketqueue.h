#ifndef AVPACKETQUEUE_H
#define AVPACKETQUEUE_H
#include "avqueue.h"

#ifdef __cplusplus
extern "C"
{
#include "libavcodec/avcodec.h"
}
#endif
class AVPacketQueue
{
public:
    AVPacketQueue();
    ~AVPacketQueue();

    int Push(AVPacket *val);
    AVPacket *Pop(const int timeout = 0);

    void Abort();
    bool isEmpty();
    int Size();
    void clear();
    void dropFront();

private:
    void release();
    AVQueue<AVPacket *> queue_;
};


#endif // AVPACKETQUEUE_H
