#ifndef VIDEOSUMMARYMANAGER_H
#define VIDEOSUMMARYMANAGER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QEventLoop>
#include <QSemaphore>
#include <atomic>
#include <QString>
#include <QList>
#include <QMap>
#include <QByteArray>
#include <QStringList>
#include "videosummarynetworkclient.h"
#include "queue/subtitlequeue.h"
#include "semanticsegmenter.h"

enum class SummaryState {
    Idle,
    ExtractingFrames,
    RunningASR,
    ClassifyingScenes,
    DetectingSemanticBoundaries,
    AnalyzingSegments,
    Stopping,
    Finished,
    Error
};

class VideoSummaryManager : public QObject {
    Q_OBJECT
public:
    explicit VideoSummaryManager(QObject* parent = nullptr);
    ~VideoSummaryManager();

    void startSummary(const QString& videoPath);
    void stopSummary();

    int segmentCount() const { return m_segments.size(); }
    const SummarySegment* segmentAt(int index) const;
    const SummaryReport& report() const { return m_fullReport; }
    const QList<SubtitleItem>& asrResults() const { return m_asrResults; }

    SummaryState state() const { return m_state; }
    int currentSegmentIndex() const { return m_currentSegment; }

    void setModel(const QString& model);

    // 缓存
    bool tryLoadFromCache(const QString& videoPath);
    void saveToCache(const QString& videoPath);
    static void clearAllCache();
    static qint64 cacheTotalSize();
    static int cacheFileCount();

    struct Progress {
        SummaryState stage = SummaryState::Idle;
        double stageProgress = 0.0;
        double overallProgress = 0.0;
        int currentSegment = 0;
        int totalSegments = 0;
    };
    Progress progress() const;

signals:
    void stateChanged(SummaryState state);
    void segmentAnalyzed(int index, const QString& description);
    void progressUpdated(double progress);
    void progressDetailChanged(const Progress& progress);
    void fullReportReady(const QString& reportJson);
    void structuredReportReady(const SummaryReport& report);
    void errorOccurred(const QString& message);
    void reportReceived(const QString& report, bool hasError, const QString& errorMsg);

private:
    void extractFrames();
    void runWhisperASR(const QString& audioPath);
    void classifyVideoScenes();
    QList<QPair<qint64, QString>> debounceSceneTags(
        const QList<QPair<qint64, QString>>& rawTags);
    void runSemanticSegmentation();
    void analyzeSegments();
    void generateFullReport();

    QByteArray extractFrameJpeg(const QString& videoPath, qint64 timestampMs);
    QString extractAudioFile(const QString& videoPath);
    void cleanupTempFiles();

    void setState(SummaryState s);

private slots:
    void runAnalysis();
    void onFrameAnalyzed(int segmentIndex, const QString& description,
                         bool hasError, const QString& errorMsg);
    void onSceneClassified(qint64 timestampMs, const QString& sceneTag,
                           bool hasError, const QString& errorMsg);
    void onReportReady(const QString& reportJson, bool hasError, const QString& errorMsg);

private:
    SummaryState m_state = SummaryState::Idle;
    QString m_videoPath;
    qint64 m_durationMs = 0;

    QList<SummarySegment> m_segments;
    QList<SubtitleItem> m_asrResults;
    SummaryReport m_fullReport;

    QMap<qint64, QByteArray> m_extractedFrames;
    QList<QPair<qint64, QString>> m_sceneTags;

    QMutex m_mutex;
    QWaitCondition m_analysisDone;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<int> m_currentSegment{0};

    int m_pendingAnalyzes = 0;
    QStringList m_pendingFrameDescs;
    bool m_pendingHasError = false;
    QString m_pendingErrorMsg;
    QString m_pendingReport;
    bool m_pendingReportReceived = false;
    QMutex m_pendingMutex;
    QWaitCondition m_pendingCond;
    QEventLoop* m_reportLoop = nullptr;
    QEventLoop* m_analysisLoop = nullptr;

    int m_pendingSceneClassifications = 0;
    QMutex m_sceneMutex;
    QWaitCondition m_sceneCond;
    std::atomic<bool> m_sceneStopRequested{false};

    SummaryNetworkClient* m_networkClient = nullptr;
    SummaryNetworkBridge* m_networkBridge = nullptr;
    QThread* m_workerThread = nullptr;
    QStringList m_tempFiles;
};

#endif // VIDEOSUMMARYMANAGER_H
