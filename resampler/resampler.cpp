#include "resampler.h"
#include <QDebug>
#include <libavutil/avstring.h>

#define ERROR_BUF \
char errbuf[AV_ERROR_MAX_STRING_SIZE];\
    av_strerror(ret, errbuf, sizeof(errbuf));

#define CODE(func, code) \
if (ret < 0) { \
        ERROR_BUF; \
        qDebug() << #func << "error: " << errbuf; \
        code; \
}

Resampler::Resampler(QObject *parent) : QObject(parent)
{
}

Resampler::~Resampler()
{
    close();
}

int Resampler::init(const AudioSpec &inSpec, const AudioSpec &outSpec)
{
    QWriteLocker locker(&mutex_);

    closeInternal();

    inSpec_ = inSpec;
    outSpec_ = outSpec;

    initChannelLayout(&inSpec_.chLayout, inSpec_.chs);
    initChannelLayout(&outSpec_.chLayout, outSpec_.chs);

    swr_alloc_set_opts2(&swrCtx_,
                        &outSpec_.chLayout, outSpec_.sampleFmt, outSpec_.sampleRate,
                        &inSpec_.chLayout, inSpec_.sampleFmt, inSpec_.sampleRate,
                        0, nullptr);

    if (!swrCtx_) {
        qDebug() << "Failed to allocate swr context";
        return AVERROR(ENOMEM);
    }

    int ret = swr_init(swrCtx_);
    CODE(swr_init, {
        swr_free(&swrCtx_);
        swrCtx_ = nullptr;
        return ret;
    });

    isInitialized_ = true;
    return 0;
}

int Resampler::resample(AVFrame *inFrame, uint8_t **outData, int *outSamples)
{
    if (!isInitialized_ || !swrCtx_ || !inFrame || !outData || !outSamples) {
        qDebug() << "[Resampler] 重采样参数无效！";
        return AVERROR(EINVAL);
    }

    QReadLocker locker(&mutex_);

    int outSamplesEst = av_rescale_rnd(
        swr_get_delay(swrCtx_, inSpec_.sampleRate) + inFrame->nb_samples,
        outSpec_.sampleRate,
        inSpec_.sampleRate,
        AV_ROUND_UP
        );

    if (outSamplesEst <= 0) {
        return AVERROR(EINVAL);
    }

    int ret = swr_convert(
        swrCtx_,
        outData,
        outSamplesEst,
        (const uint8_t **)inFrame->data,
        inFrame->nb_samples
        );
    CODE(swr_convert, {
        return ret;
    });

    *outSamples = ret;
    return 0;
}

void Resampler::close()
{
    QWriteLocker locker(&mutex_);
    closeInternal();
}

SwrContext *Resampler::getSwrContext() const
{
    QReadLocker locker(&mutex_);
    return swrCtx_;
}

void Resampler::closeInternal()
{
    if (swrCtx_) {
        swr_free(&swrCtx_);
        swrCtx_ = nullptr;
    }
    isInitialized_ = false;
}

Resampler::AudioSpec Resampler::getInputSpec() const
{
    QReadLocker locker(&mutex_);
    return inSpec_;
}

Resampler::AudioSpec Resampler::getOutputSpec() const
{
    QReadLocker locker(&mutex_);
    return outSpec_;
}

int Resampler::getOutputBufferSize(int samples) const
{
    QReadLocker locker(&mutex_);
    if (!swrCtx_) {
        return 0;
    }

    int size = av_samples_get_buffer_size(nullptr, outSpec_.chs, samples, outSpec_.sampleFmt, 1);
    return size;
}

void Resampler::initChannelLayout(AVChannelLayout *chLayout, int chs)
{
    if (!chLayout) return;

    switch (chs) {
    case 1:
        av_channel_layout_from_string(chLayout, "mono");
        break;
    case 2:
        av_channel_layout_from_string(chLayout, "stereo");
        break;
    case 4:
        av_channel_layout_from_string(chLayout, "quad");
        break;
    case 6:
        av_channel_layout_from_string(chLayout, "5.1");
        break;
    default:
        av_channel_layout_default(chLayout, chs);
        break;
    }
}
