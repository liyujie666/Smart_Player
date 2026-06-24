
#include "packetpool.h"
#include <QDebug>
PacketPool::PacketPool(size_t maxSize) : maxSize_(maxSize) {
    for (auto& p : ring_) p = nullptr;
}

PacketPool::~PacketPool() {
    clear();
}

AVPacket *PacketPool::get() {
    size_t r = ringRead_.load(std::memory_order_acquire);
    if (r != ringWrite_.load(std::memory_order_acquire)) {
        AVPacket* pkt = ring_[r % kRingCapacity];
        ring_[r % kRingCapacity] = nullptr;
        ringRead_.store(r + 1, std::memory_order_release);
        av_packet_unref(pkt);
        getCount_++;
        return pkt;
    }
    createCount_++;
    return av_packet_alloc();
}

void PacketPool::recycle(AVPacket *pkt) {
    if (!pkt) return;
    av_packet_unref(pkt);
    size_t w = ringWrite_.load(std::memory_order_relaxed);
    size_t r = ringRead_.load(std::memory_order_relaxed);
    size_t count = (w >= r) ? (w - r) : (kRingCapacity - r + w);
    if (count >= maxSize_) {
        freeCount_++;
        av_packet_free(&pkt);
    } else {
        ring_[w % kRingCapacity] = pkt;
        ringWrite_.store(w + 1, std::memory_order_release);
        recycleCount_++;
    }
}

void PacketPool::clear() {
    size_t r = ringRead_.load(std::memory_order_acquire);
    size_t w = ringWrite_.load(std::memory_order_acquire);
    while (r != w) {
        AVPacket* pkt = ring_[r % kRingCapacity];
        if (pkt) {
            av_packet_free(&pkt);
            freed_++;
        }
        r++;
    }
    ringRead_.store(0, std::memory_order_release);
    ringWrite_.store(0, std::memory_order_release);
}

void PacketPool::setMaxSize(size_t newMaxSize) {
    if (newMaxSize == 0) return;
    maxSize_ = newMaxSize;
}

size_t PacketPool::maxSize() const {
    return maxSize_;
}

void PacketPool::printStats() {
    size_t r = ringRead_.load(std::memory_order_relaxed);
    size_t w = ringWrite_.load(std::memory_order_relaxed);
    size_t inPool = (w >= r) ? (w - r) : (kRingCapacity - r + w);
    qDebug() << "[PacketPool] Allocated:" << createCount_.load()
             << " Get:" << getCount_.load()
             << " Recycled:" << recycleCount_.load()
             << " Freed:" << freeCount_.load()
             << " In Pool:" << inPool;
}
