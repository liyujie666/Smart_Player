#ifndef PACKETPOOL_H
#define PACKETPOOL_H

#include <queue>
#include <mutex>
#include <atomic>
extern "C" {
#include <libavcodec/avcodec.h>
}

class PacketPool {
public:
    PacketPool(size_t maxSize = 128);
    ~PacketPool();

    AVPacket* get();
    void recycle(AVPacket* pkt);

    void setMaxSize(size_t newMaxSize);
    size_t getMaxSize();
    void clear();
    int getCreateCount() const { return createCount_; }
    int getRecycleCount() const { return recycleCount_; }
    int getCount() const { return getCount_; }

    void printStats();
private:
    std::queue<AVPacket*> pool_;
    std::mutex mutex_;
    size_t maxSize_;

    std::atomic<int> createCount_{0};
    std::atomic<int> recycleCount_{0};
    std::atomic<int> getCount_{0};
    std::atomic<int> freeCount_{0};
    std::atomic<int> freed_{0};
};
#endif // PACKETPOOL_H
