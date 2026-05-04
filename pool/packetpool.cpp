#include "packetpool.h"
#include <QDebug>
PacketPool::PacketPool(size_t maxSize) : maxSize_(maxSize) {}

PacketPool::~PacketPool() {
    clear();
}

AVPacket *PacketPool::get()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!pool_.empty())
    {
        AVPacket* pkt = pool_.front();
        pool_.pop();
        av_packet_unref(pkt);
        getCount_++;
        return pkt;
    }
    createCount_++;
    return av_packet_alloc();
}

void PacketPool::recycle(AVPacket *pkt)
{
    if (!pkt) return;
    std::lock_guard<std::mutex> lock(mutex_);
    av_packet_unref(pkt);
    if (pool_.size() >= maxSize_) {
        freeCount_++;
        av_packet_free(&pkt);
    } else {
        recycleCount_++;
        pool_.push(pkt);
    }

}
void PacketPool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    while (!pool_.empty()) {
        AVPacket* pkt = pool_.front();
        pool_.pop();
        av_packet_free(&pkt);
        freed_++;
    }
}

void PacketPool::setMaxSize(size_t newMaxSize)
{
    if (newMaxSize <= 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    maxSize_ = newMaxSize;
    while (pool_.size() > maxSize_) {
        AVPacket* pkt = pool_.front();
        pool_.pop();
        av_packet_free(&pkt);
        freeCount_++;
    }
}

size_t PacketPool::getMaxSize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return maxSize_;
}

void PacketPool::printStats()
{
    std::lock_guard<std::mutex> lock(mutex_);
    qDebug() << "[PacketPool] Allocated:" << createCount_
             << " Get:" << getCount_
             << " Recycled:" << recycleCount_
             << "Freed:" << freeCount_
             << " In Pool:" << pool_.size();
}

