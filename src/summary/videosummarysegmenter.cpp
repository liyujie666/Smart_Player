#include "videosummarysegmenter.h"

QList<SummarySegment> SummarySegmenter::segmentByDuration(
    qint64 totalDurationMs, qint64 segmentDurationMs) {

    QList<SummarySegment> segments;
    for (qint64 t = 0; t < totalDurationMs; t += segmentDurationMs) {
        SummarySegment seg;
        seg.index = segments.size();
        seg.startMs = t;
        seg.endMs = qMin(t + segmentDurationMs, totalDurationMs);
        seg.isAnalyzed = false;
        segments.append(seg);
    }
    return segments;
}

void SummarySegmenter::aggregateSpeechText(
    QList<SummarySegment>& segments,
    const QList<SubtitleItem>& asrResults) {

    for (auto& seg : segments) {
        QStringList words;
        for (const auto& item : asrResults) {
            qint64 startMs = qRound(item.start_sec * 1000.0);
            qint64 endMs = qRound(item.end_sec * 1000.0);
            if (endMs > seg.startMs && startMs < seg.endMs) {
                words.append(QString::fromStdString(item.text));
            }
        }
        seg.speechText = words.join(" ");
    }
}

QList<qint64> SummarySegmenter::sampleTimestamps(qint64 startMs, qint64 endMs, int count) {
    QList<qint64> timestamps;
    if (count <= 0 || startMs >= endMs) return timestamps;

    double duration = (endMs - startMs) / 1000.0;
    for (int i = 1; i <= count; ++i) {
        double t = (startMs / 1000.0) + (duration * i / (count + 1));
        timestamps.append(qRound(t * 1000));
    }
    return timestamps;
}
