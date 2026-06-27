
#include "framepool.h"
#include <QDebug>
FramePool::FramePool(size_t maxSize) : maxSize_(maxSize) {
    for (auto& p : ring_) p = nullptr;
}

FramePool::~FramePool() {
    clear();
}

AVFrame* FramePool::get() {
    size_t r = ringRead_.load(std::memory_order_acquire);
    if (r != ringWrite_.load(std::memory_order_acquire)) {
        AVFrame* frame = ring_[r % kRingCapacity];
        ring_[r % kRingCapacity] = nullptr;
        ringRead_.store(r + 1, std::memory_order_release);
        av_frame_unref(frame);
        getCount_++;
        return frame;
    }
    createCount_++;
    return av_frame_alloc();
}

void FramePool::recycle(AVFrame* frame) {
    if (!frame) return;
    av_frame_unref(frame);
    size_t w = ringWrite_.load(std::memory_order_relaxed);
    size_t r = ringRead_.load(std::memory_order_relaxed);
    size_t count = (w >= r) ? (w - r) : (kRingCapacity - r + w);
    if (count >= maxSize_) {
        freeCount_++;
        av_frame_free(&frame);
    } else {
        ring_[w % kRingCapacity] = frame;
        ringWrite_.store(w + 1, std::memory_order_release);
        recycleCount_++;
    }
}

void FramePool::setMaxSize(size_t newMaxSize) {
    if (newMaxSize == 0) return;
    maxSize_ = newMaxSize;
}

size_t FramePool::maxSize() const {
    return maxSize_;
}

void FramePool::clear() {
    size_t r = ringRead_.load(std::memory_order_acquire);
    size_t w = ringWrite_.load(std::memory_order_acquire);
    while (r != w) {
        AVFrame* frame = ring_[r % kRingCapacity];
        if (frame) {
            av_frame_free(&frame);
            freed_++;
        }
        r++;
    }
    ringRead_.store(0, std::memory_order_release);
    ringWrite_.store(0, std::memory_order_release);
}

void FramePool::printStats() {
    size_t r = ringRead_.load(std::memory_order_relaxed);
    size_t w = ringWrite_.load(std::memory_order_relaxed);
    size_t inPool = (w >= r) ? (w - r) : (kRingCapacity - r + w);
    qDebug() << "[FramePool] Allocated:" << createCount_.load()
             << " Get:" << getCount_.load()
             << " Recycled:" << recycleCount_.load()
             << " Freed:" << freeCount_.load()
             << " In Pool:" << inPool;
}
