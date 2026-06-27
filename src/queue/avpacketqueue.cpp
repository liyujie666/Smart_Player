#include "avpacketqueue.h"
#include "utils/log.h"
#include "pool/gloabalpool.h"
AVPacketQueue::AVPacketQueue()
{

}

AVPacketQueue::~AVPacketQueue()
{
    Abort();
}

void AVPacketQueue::Abort()
{
    release();
    queue_.Abort();
}


int AVPacketQueue::Size()
{
    return queue_.Size();
}

bool AVPacketQueue::isEmpty()
{
    return queue_.isEmpty();
}

int AVPacketQueue::Push(AVPacket *val)
{
    if (!val) return -1;

    AVPacket* tmp = GlobalPool::getPacketPool().get();
    if (!tmp) return -1;

    av_packet_move_ref(tmp, val);

    int ret = queue_.Push(tmp);
    if (ret < 0) {
        GlobalPool::getPacketPool().recycle(tmp);
    }
    return ret;
}

AVPacket *AVPacketQueue::Pop(const int timeout)
{
    AVPacket *tmp_pkt = NULL;
    int ret = queue_.Pop(tmp_pkt, timeout);
    if (ret == -1) {
        LOG_ERROR("AVPacketQueue::Pop aborted");
    }
    return tmp_pkt;
}

void AVPacketQueue::release() {
    AVPacket *pkt = nullptr;

    while (queue_.Pop(pkt, 0) == 0) {
        //av_packet_free(&pkt);
        GlobalPool::getPacketPool().recycle(pkt);
    }
    queue_.clear();
}

void AVPacketQueue::clear() {
    release();
}

void AVPacketQueue::dropFront() {
    AVPacket *pkt = Pop(0);
    if (pkt) {
        //av_packet_free(&pkt);
        GlobalPool::getPacketPool().recycle(pkt);
    }
}
