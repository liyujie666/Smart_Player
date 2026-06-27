#ifndef GLOABALPOOL_H
#define GLOABALPOOL_H
#include "pool/framepool.h"
#include "pool/packetpool.h"

class GlobalPool {
public:
    static PacketPool& getPacketPool() {
        static PacketPool pool(108);
        return pool;
    }

    static FramePool& getFramePool() {
        static FramePool pool(24);
        return pool;
    }

    static void setFramePoolMaxSize(size_t newMaxSize) {
        getFramePool().setMaxSize(newMaxSize);
    }

    static void setPacketPoolMaxSize(size_t newMaxSize) {
        getPacketPool().setMaxSize(newMaxSize);
    }

    static size_t getFramePoolMaxSize() {
        return getFramePool().maxSize();
    }

    static size_t getPacketPoolMaxSize() {
        return getPacketPool().maxSize();
    }
};
#endif // GLOABALPOOL_H
