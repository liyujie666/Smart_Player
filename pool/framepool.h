#ifndef FRAMEPOOL_H
#define FRAMEPOOL_H

#include <queue>
#include <mutex>
#include <atomic>
extern "C" {
#include <libavutil/frame.h>
}

class FramePool {
public:
    FramePool(size_t maxSize = 128);
    ~FramePool();

    AVFrame* get();
    void recycle(AVFrame* frame);

    void setMaxSize(size_t newMaxSize);
    size_t getMaxSize();
    void clear();
    void printStats();

private:
    std::queue<AVFrame*> pool_;
    std::mutex mutex_;
    size_t maxSize_;

    std::atomic<int> createCount_{0};
    std::atomic<int> recycleCount_{0};
    std::atomic<int> getCount_{0};
    std::atomic<int> freeCount_{0};
    std::atomic<int> freed_{0};
};

#endif // FRAMEPOOL_H
