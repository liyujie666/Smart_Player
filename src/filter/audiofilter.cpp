#include "audiofilter.h"
#include <QDebug>
#include <cstring>

AudioFilter::AudioFilter(QObject *parent)
    : QObject(parent),
    currentSpeedIndex_(static_cast<int>(SpeedIndex::Speed_1_0))
{
    av_channel_layout_uninit(&chLayout_);
}

AudioFilter::~AudioFilter() {
    close();
    av_channel_layout_uninit(&chLayout_);
}

int AudioFilter::init(int sampleRate, AVSampleFormat sampleFmt, int chs) {
    QMutexLocker locker(&mutex_);
    closeInternal();

    if (sampleRate <= 0 || sampleFmt == AV_SAMPLE_FMT_NONE || chs <= 0) {
        qDebug() << "AudioFilter init invalid params";
        return -1;
    }

    sampleRate_ = sampleRate;
    sampleFmt_ = sampleFmt;

    av_channel_layout_uninit(&chLayout_);
    switch (chs) {
    case 1: av_channel_layout_from_string(&chLayout_, "mono"); break;
    case 2: av_channel_layout_from_string(&chLayout_, "stereo"); break;
    case 4: av_channel_layout_from_string(&chLayout_, "quad"); break;
    case 6: av_channel_layout_from_string(&chLayout_, "5.1"); break;
    default: av_channel_layout_default(&chLayout_, chs); break;
    }

    char buf[64] = {0};
    int ret = av_channel_layout_describe(&chLayout_, buf, sizeof(buf));
    if (ret < 0) {
        qDebug() << "av_channel_layout_describe failed";
        return -1;
    }
    chLayoutStr_ = buf;

    if (createSingleFilter(static_cast<int>(SpeedIndex::Speed_0_5), 0.5) < 0) {
        closeInternal();
        return -1;
    }
    if (createSingleFilter(static_cast<int>(SpeedIndex::Speed_1_0), 1.0) < 0) {
        closeInternal();
        return -1;
    }
    if (createSingleFilter(static_cast<int>(SpeedIndex::Speed_1_5), 1.5) < 0) {
        closeInternal();
        return -1;
    }
    if (createSingleFilter(static_cast<int>(SpeedIndex::Speed_2_0), 2.0) < 0) {
        closeInternal();
        return -1;
    }

    isInitialized_ = true;
    qDebug() << "AudioFilter init success";
    return 0;
}

int AudioFilter::createSingleFilter(int index, double speed) {
    FilterGroup &g = groups_[index];
    g.graph = avfilter_graph_alloc();
    if (!g.graph) {
        qDebug() << "avfilter_graph_alloc failed index:" << index;
        return -1;
    }

    // 构建源参数
    std::string srcArgs = "sample_rate=" + std::to_string(sampleRate_) +
                          ":sample_fmt=" + av_get_sample_fmt_name(sampleFmt_) +
                          ":channel_layout=" + chLayoutStr_;

    // std::string aformatArgs = "sample_fmts=" + std::string(av_get_sample_fmt_name(sampleFmt_)) +
    //                           ":sample_rates=" + std::to_string(sampleRate_) +
    //                           ":channel_layouts=" + chLayoutStr_;
    std::string aformatArgs = "sample_fmts=s16:sample_rates=" + std::to_string(sampleRate_) +
                  ":channel_layouts=" + chLayoutStr_;

    const AVFilter *srcFilter = avfilter_get_by_name("abuffer");
    g.srcCtx = avfilter_graph_alloc_filter(g.graph, srcFilter, "src");
    if (!g.srcCtx || avfilter_init_str(g.srcCtx, srcArgs.c_str()) < 0) {
        qDebug() << "abuffer init failed:" << srcArgs.c_str();
        avfilter_graph_free(&g.graph);
        g.graph = nullptr;
        return -1;
    }

    const AVFilter *atempoFilter = avfilter_get_by_name("atempo");
    AVFilterContext *atempoCtx = avfilter_graph_alloc_filter(g.graph, atempoFilter, "atempo");
    AVDictionary *dict = nullptr;
    av_dict_set(&dict, "tempo", std::to_string(speed).c_str(), 0);
    if (!atempoCtx || avfilter_init_dict(atempoCtx, &dict) < 0) {
        qDebug() << "atempo init failed speed:" << speed;
        av_dict_free(&dict);
        avfilter_graph_free(&g.graph);
        g.graph = nullptr;
        return -1;
    }
    av_dict_free(&dict);

    const AVFilter *aformatFilter = avfilter_get_by_name("aformat");
    AVFilterContext *aformatCtx = avfilter_graph_alloc_filter(g.graph, aformatFilter, "aformat");
    if (avfilter_init_str(aformatCtx, aformatArgs.c_str()) < 0) {
        qDebug() << "aformat init failed!";
        avfilter_graph_free(&g.graph);
        g.graph = nullptr;
        return -1;
    }

    const AVFilter *sinkFilter = avfilter_get_by_name("abuffersink");
    g.sinkCtx = avfilter_graph_alloc_filter(g.graph, sinkFilter, "sink");
    if (!g.sinkCtx || avfilter_init_dict(g.sinkCtx, nullptr) < 0) {
        qDebug() << "abuffersink init failed";
        avfilter_graph_free(&g.graph);
        g.graph = nullptr;
        return -1;
    }

    if (avfilter_link(g.srcCtx,   0, atempoCtx,   0) != 0 ||
        avfilter_link(atempoCtx,  0, aformatCtx, 0) != 0 ||
        avfilter_link(aformatCtx, 0, g.sinkCtx,  0) != 0) {
        qDebug() << "filter link failed";
        avfilter_graph_free(&g.graph);
        g.graph = nullptr;
        return -1;
    }

    if (avfilter_graph_config(g.graph, nullptr) < 0) {
        qDebug() << "avfilter_graph_config failed";
        avfilter_graph_free(&g.graph);
        g.graph = nullptr;
        return -1;
    }

    qDebug() << "Filter create success index:" << index << "speed:" << speed;
    return 0;
}

int AudioFilter::process(AVFrame *inFrame, AVFrame *outFrame) {
    QMutexLocker locker(&mutex_);

    if (!isInitialized_ || !inFrame || !outFrame) {
        return -1;
    }

    if (inFrame->sample_rate != sampleRate_ || inFrame->format != sampleFmt_ ||
        av_channel_layout_compare(&inFrame->ch_layout, &chLayout_) != 0) {
        qDebug() << "Input frame format mismatch!";
        return -1;
    }

    av_frame_unref(outFrame);
    int idx = currentSpeedIndex_;
    //qDebug() << "当前倍速：currentSpeedIndex_" << idx;
    if (idx < 1 || idx > 4) return -1;

    FilterGroup &g = groups_[idx];
    if (!g.srcCtx || !g.sinkCtx) return -1;

    int ret = av_buffersrc_add_frame(g.srcCtx, inFrame);
    if (ret < 0) return ret;

    return av_buffersink_get_frame(g.sinkCtx, outFrame);
}

void AudioFilter::setSpeedIndex(SpeedIndex index) {
    currentSpeedIndex_ = static_cast<int>(index);
}

void AudioFilter::closeInternal() {
    for (int i = 1; i <= 4; ++i) {
        if (groups_[i].graph) {
            avfilter_graph_free(&groups_[i].graph);
        }
        groups_[i].srcCtx = nullptr;
        groups_[i].sinkCtx = nullptr;
    }
    isInitialized_ = false;
}

void AudioFilter::close() {
    QMutexLocker locker(&mutex_);
    closeInternal();
}

void AudioFilter::flush() {
    QMutexLocker locker(&mutex_);
    if (!isInitialized_) return;

    int idx = currentSpeedIndex_;
    if (idx < 1 || idx > 4) return;
    FilterGroup &g = groups_[idx];
    if (!g.srcCtx || !g.sinkCtx) return;

    int rc = av_buffersrc_add_frame(g.srcCtx, nullptr);
    if (rc < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(rc, errbuf, sizeof(errbuf));
        qDebug() << "AudioFilter::flush av_buffersrc_add_frame failed:" << errbuf;
    }

    AVFrame *frame = av_frame_alloc();
    if (frame) {
        for (int i = 0; i < 10; ++i) {
            if (av_buffersink_get_frame(g.sinkCtx, frame) < 0) break;
            av_frame_unref(frame);
        }
        av_frame_free(&frame);
    }
}

double AudioFilter::speedValue() const {
    switch (currentSpeedIndex_.load()) {
    case static_cast<int>(SpeedIndex::Speed_0_5):    return 0.5;
    case static_cast<int>(SpeedIndex::Speed_1_0):    return 1.0;
    case static_cast<int>(SpeedIndex::Speed_1_5):    return 1.5;
    case static_cast<int>(SpeedIndex::Speed_2_0):    return 2.0;
    default:           return 1.0;
    }
}

AudioFilter::SpeedIndex AudioFilter::speedIndex() const {
    return static_cast<SpeedIndex>(currentSpeedIndex_.load());
}

bool AudioFilter::isInitialized() const
{
    return isInitialized_;
}
