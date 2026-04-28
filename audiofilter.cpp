#include "audiofilter.h"
#include <QDebug>
#include <string>

AudioFilter::AudioFilter(QObject *parent)
    : QObject(parent),
    currentSpeedIndex_(Speed_1_0)
{
}

AudioFilter::~AudioFilter() {
    close();
}

int AudioFilter::init(int sampleRate, AVSampleFormat sampleFmt, int chs) {
    closeInternal();
    sampleRate_ = sampleRate;
    sampleFmt_ = sampleFmt;
    ch_layout_ = chs;

    if (createSingleFilter(Speed_0_5, 0.5) < 0) return -1;
    if (createSingleFilter(Speed_1_0, 1.0) < 0) return -1;
    if (createSingleFilter(Speed_1_5, 1.5) < 0) return -1;
    if (createSingleFilter(Speed_2_0, 2.0) < 0) return -1;
    return 0;
}

int AudioFilter::createSingleFilter(int index, double speed) {
    FilterGroup &g = groups_[index];
    g.graph = avfilter_graph_alloc();
    if (!g.graph) return -1;

    // 源滤镜参数
    std::string srcArgs = std::string("sample_rate=") + std::to_string(sampleRate_) +
                          ":sample_fmt=" + av_get_sample_fmt_name(sampleFmt_) +
                          ":channel_layout=" + std::to_string(ch_layout_);

    // 创建源滤镜 abuffer
    const AVFilter *srcFilter = avfilter_get_by_name("abuffer");
    g.srcCtx = avfilter_graph_alloc_filter(g.graph, srcFilter, "src");
    if (avfilter_init_str(g.srcCtx, srcArgs.c_str()) < 0) {
        qDebug() << "源滤镜初始化失败";
        return -1;
    }

    // 创建 atempo 变速滤镜
    const AVFilter *atempoFilter = avfilter_get_by_name("atempo");
    AVFilterContext *atempoCtx = avfilter_graph_alloc_filter(g.graph, atempoFilter, "atempo");
    AVDictionary *dict = nullptr;
    av_dict_set(&dict, "tempo", std::to_string(speed).c_str(), 0);
    if (avfilter_init_dict(atempoCtx, &dict) < 0) {
        qDebug() << "atempo初始化失败";
        av_dict_free(&dict);
        return -1;
    }
    av_dict_free(&dict); // 释放字典

    // 创建输出滤镜 abuffersink
    const AVFilter *sinkFilter = avfilter_get_by_name("abuffersink");
    g.sinkCtx = avfilter_graph_alloc_filter(g.graph, sinkFilter, "sink");
    if (avfilter_init_dict(g.sinkCtx, nullptr) < 0) {
        qDebug() << "sink初始化失败";
        return -1;
    }

    // 简化链接：源 → 变速 → 输出
    if (avfilter_link(g.srcCtx, 0, atempoCtx, 0) != 0 ||
        avfilter_link(atempoCtx, 0, g.sinkCtx, 0) != 0) {
        qDebug() << "滤镜链接失败";
        return -1;
    }

    // 配置滤镜图
    if (avfilter_graph_config(g.graph, nullptr) < 0) {
        qDebug() << "滤镜图配置失败";
        return -1;
    }

    qDebug() << "滤镜创建成功 index:" << index << "speed:" << speed;
    return 0;
}

int AudioFilter::process(AVFrame *inFrame, AVFrame *outFrame) {
    av_frame_unref(outFrame);
    int idx = currentSpeedIndex_;
    if (idx < 1 || idx > 4) return -1;

    FilterGroup &g = groups_[idx];
    if (!g.srcCtx || !g.sinkCtx) return -1;

    // 标准送帧/取帧
    int ret = av_buffersrc_add_frame(g.srcCtx, inFrame);
    if (ret < 0) return ret;

    return av_buffersink_get_frame(g.sinkCtx, outFrame);
}

void AudioFilter::setSpeedIndex(SpeedIndex index) {
    currentSpeedIndex_ = index;
}

void AudioFilter::closeInternal() {
    for (int i = 1; i <= 4; ++i) {
        if (groups_[i].graph) {
            avfilter_graph_free(&groups_[i].graph);
        }
        groups_[i].srcCtx = nullptr;
        groups_[i].sinkCtx = nullptr;
    }
}

void AudioFilter::close() {
    closeInternal();
}

void AudioFilter::flush() {
    int idx = currentSpeedIndex_;
    if (idx < 1 || idx > 4) return;
    FilterGroup &g = groups_[idx];
    if (!g.srcCtx || !g.sinkCtx) return;

    // 向滤镜送入空帧，刷新内部缓冲区
    av_buffersrc_add_frame(g.srcCtx, nullptr);
    // 清空所有缓存数据
    AVFrame *frame = av_frame_alloc();
    while (av_buffersink_get_frame(g.sinkCtx, frame) >= 0) {
        av_frame_unref(frame);
    }
    av_frame_free(&frame);
}

