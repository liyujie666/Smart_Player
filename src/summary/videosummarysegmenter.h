#ifndef VIDEOSUMMARYSEGMENTER_H
#define VIDEOSUMMARYSEGMENTER_H

#include "videosummarynetworkclient.h"
#include "queue/subtitlequeue.h"
#include <QList>
#include <QStringList>

class SummarySegmenter {
public:
    static QList<SummarySegment> segmentByDuration(qint64 totalDurationMs,
                                                   qint64 segmentDurationMs = 5000);

    static void aggregateSpeechText(QList<SummarySegment>& segments,
                                   const QList<SubtitleItem>& asrResults);

    static QList<qint64> sampleTimestamps(qint64 startMs, qint64 endMs, int count);
};

#endif // VIDEOSUMMARYSEGMENTER_H
