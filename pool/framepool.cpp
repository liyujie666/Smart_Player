#include "framepool.h"
#include <QDebug>
FramePool::FramePool(size_t maxSize) : maxSize_(maxSize) {}

FramePool::~FramePool() {
    clear();
}

AVFrame* FramePool::get() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pool_.empty()) {
        AVFrame* frame = pool_.front();
        pool_.pop();
        av_frame_unref(frame);
        getCount_++;
        return frame;
    }
    createCount_++;
    return av_frame_alloc();
}

void FramePool::recycle(AVFrame* frame) {
    if (!frame) return;
    std::lock_guard<std::mutex> lock(mutex_);
    av_frame_unref(frame);
    if (pool_.size() >= maxSize_) {
        freeCount_++;
        av_frame_free(&frame);
    } else {
        pool_.push(frame); 
        recycleCount_++;
    }
}

void FramePool::setMaxSize(size_t newMaxSize)
{
    if (newMaxSize <= 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    maxSize_ = newMaxSize;
    while (pool_.size() > maxSize_) {
        AVFrame* frame = pool_.front();
        pool_.pop();
        av_frame_free(&frame);
        freeCount_++;
    }

}

size_t FramePool::getMaxSize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return maxSize_;
}

void FramePool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!pool_.empty()) {
        AVFrame* frame = pool_.front();
        pool_.pop();
        if (!frame || (frame->data[0] == nullptr && frame->buf[0] == nullptr)) {
            continue;
        }
        av_frame_free(&frame);
        freed_++;
    }
}

void FramePool::printStats()
{
    std::lock_guard<std::mutex> lock(mutex_);
    qDebug() << "[FramePool] Allocated:" << createCount_
             << " Get:" << getCount_
             << " Recycled:" << recycleCount_
             << "Freed:" << freeCount_
             << " In Pool:" << pool_.size();
}
