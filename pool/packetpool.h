#ifndef PACKETPOOL_H
#define PACKETPOOL_H

#include <atomic>
#include <cstddef>
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
    size_t maxSize() const;
    void clear();
    int getCreateCount() const { return createCount_; }
    int getRecycleCount() const { return recycleCount_; }
    int getCount() const { return getCount_; }

    void printStats();
private:
    static constexpr size_t kRingCapacity = 256;

    AVPacket* ring_[kRingCapacity];
    std::atomic<size_t> ringWrite_{0};
    std::atomic<size_t> ringRead_{0};
    size_t maxSize_;

    std::atomic<int> createCount_{0};
    std::atomic<int> recycleCount_{0};
    std::atomic<int> getCount_{0};
    std::atomic<int> freeCount_{0};
    std::atomic<int> freed_{0};
};
#endif // PACKETPOOL_H
