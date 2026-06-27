#ifndef RESAMPLER_H
#define RESAMPLER_H

#include <QObject>
#include <QReadWriteLock> // 替换QMutex，性能优化
#include <atomic>

extern "C"
{
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

class Resampler : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Resampler)

public:

    typedef struct {
        int sampleRate;
        enum AVSampleFormat sampleFmt;
        AVChannelLayout chLayout;
        int chs;
        int bytesPerSample;
    } AudioSpec;

    explicit Resampler(QObject *parent = nullptr);
    ~Resampler();


    int init(const AudioSpec &inSpec, const AudioSpec &outSpec);
    int resample(AVFrame *inFrame, uint8_t **outData, int *outSamples);
    void close();

    AudioSpec inputSpec() const;
    AudioSpec outputSpec() const;
    SwrContext* swrContext() const;
    int outputBufferSize(int samples) const;

private:
    void initChannelLayout(AVChannelLayout *chLayout, int chs);
    void closeInternal();

private:
    SwrContext *swrCtx_ = nullptr;
    AudioSpec inSpec_;
    AudioSpec outSpec_;


    mutable QReadWriteLock mutex_;
    std::atomic<bool> isInitialized_{false};
};

#endif // RESAMPLER_H
