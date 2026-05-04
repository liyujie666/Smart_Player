#ifndef IASRSTRATEGY_H
#define IASRSTRATEGY_H

#include <QString>
#include "queue/subtitlequeue.h"

extern"C"{
#include <libavformat/avformat.h>
}

class IAsrStrategy {
public:
    virtual ~IAsrStrategy() = default;
    virtual bool init(const QString& url, AVStream* audio, SubtitleQueue* queue) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual void sendAudio(AVFrame*) {}
    virtual void release() = 0;
};

#endif
