#ifndef SEMANTICSEGMENTER_H
#define SEMANTICSEGMENTER_H

#include "queue/subtitlequeue.h"
#include "videosummarynetworkclient.h"
#include <QList>
#include <QVector>
#include <QString>
#include <QMap>
#include <cmath>

struct SemanticBoundary {
    qint64 timestampMs = 0;
    double audioScore = 0.0;
    double videoScore = 0.0;
    double fusedScore = 0.0;
    QString audioReason;
    QString videoReason;
};

class SemanticSegmenter {
public:
    struct Config {
        double audioWeight = 0.6;
        double videoWeight = 0.4;
        double similarityThreshold = 0.3;
        int minSegmentMs = 3000;
        int maxSegmentMs = 120000;
        int mergeWindowMs = 2000;
        int depthScoreMultiplier = 15;
    };

    explicit SemanticSegmenter() = default;

    void setConfig(const Config& cfg) { m_config = cfg; }
    Config config() const { return m_config; }

    QList<qint64> computeSegments(
        const QList<SubtitleItem>& asrResults,
        qint64 totalDurationMs,
        const QList<QPair<qint64, QString>>& videoSceneTags);

    QList<SemanticBoundary> detectAudioBoundaries(
        const QList<SubtitleItem>& asrResults);

    QList<SemanticBoundary> detectVideoBoundaries(
        const QList<QPair<qint64, QString>>& sceneTags);

    QList<qint64> fuseAndSegment(
        const QList<SemanticBoundary>& audioBoundaries,
        const QList<SemanticBoundary>& videoBoundaries,
        qint64 totalDurationMs);

    QList<SemanticBoundary> allBoundaries() const { return m_allBoundaries; }

    static void boundariesToSegments(
        const QList<qint64>& boundaries,
        QList<SummarySegment>& outSegments);

    static void aggregateSpeechText(
        QList<SummarySegment>& segments,
        const QList<SubtitleItem>& asrResults);

    static QList<qint64> sampleTimestamps(
        qint64 startMs, qint64 endMs, int count);

private:
    QVector<double> computeSentenceSimilarity(
        const QList<SubtitleItem>& asrResults);

    QList<qint64> textTiling(
        const QList<SubtitleItem>& asrResults,
        const QVector<double>& similarities);

    QVector<double> tfidfVector(const QString& text);

    double cosineSimilarity(const QVector<double>& a, const QVector<double>& b);

    QList<qint64> adaptiveSegmentLength(
        const QList<qint64>& rawBoundaries,
        qint64 totalDurationMs);

    Config m_config;
    QList<SemanticBoundary> m_allBoundaries;
};

#endif
