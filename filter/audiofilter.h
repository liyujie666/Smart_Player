#ifndef AUDIOFILTER_H
#define AUDIOFILTER_H

#include <QObject>
#include <atomic>
#include <string>
#include <QMutex> // 新增：线程安全锁

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h> // FFmpeg7.1必备
}

class AudioFilter : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(AudioFilter)

public:
    enum class SpeedIndex : int {
        Speed_0_5 = 1,
        Speed_1_0 = 2,
        Speed_1_5 = 3,
        Speed_2_0 = 4
    };

    explicit AudioFilter(QObject *parent = nullptr);
    ~AudioFilter();

    int init(int sampleRate, AVSampleFormat sampleFmt, int chs);
    int process(AVFrame *inFrame, AVFrame *outFrame);
    void setSpeedIndex(SpeedIndex index);
    void close();
    void flush();

    double speedValue() const;
    SpeedIndex speedIndex() const;
    bool isInitialized() const;

private:
    int createSingleFilter(int index, double speed);
    void buildChannelLayoutString();
    void closeInternal();

private:
    struct FilterGroup {
        AVFilterGraph *graph = nullptr;
        AVFilterContext *srcCtx = nullptr;
        AVFilterContext *sinkCtx = nullptr;
    };

    FilterGroup groups_[5];
    std::atomic<int> currentSpeedIndex_;
    int sampleRate_;
    AVSampleFormat sampleFmt_;
    AVChannelLayout chLayout_;
    std::string chLayoutStr_;

    std::atomic<bool> isInitialized_{false};
    mutable QMutex mutex_;

};

#endif
