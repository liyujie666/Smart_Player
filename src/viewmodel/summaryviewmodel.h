#ifndef SUMMARYVIEWMODEL_H
#define SUMMARYVIEWMODEL_H

#include "iviewmodel.h"
#include "summary/videosummarymanager.h"

/**
 *  SummaryViewModel
 *
 *  封装 VideoSummaryManager，对 View 仅暴露：
 *    - 可观察属性（Q_PROPERTY + NOTIFY）：state / overallProgress / stageProgress /
 *      currentSegment / totalSegments / videoPath / hasReport / lastErrorMessage
 *    - 命令槽（public slots）：setVideoPath / start / stop / rerun /
 *      exportMarkdownTo / setModel / clearCache
 *    - 数据访问：report() / segments() / asrResults() / segmentAt() 透传给 manager
 *
 *  设计要点：
 *  - VM **拥有** VideoSummaryManager（manager 的父对象设为本 VM）。
 *  - View（SummaryPanel）禁止再 #include "videosummarymanager.h"；
 *    所有交互通过 SummaryViewModel 进行。
 *  - VM 与 PlayerViewModel/TranscriptViewModel **不直接交互**，由
 *    Controller（MainWindow）通过 reportChanged / seekRequested 等信号协调。
 */
class SummaryViewModel : public IViewModel {
    Q_OBJECT
    Q_PROPERTY(SummaryState state            READ state            NOTIFY stateChanged)
    Q_PROPERTY(double       overallProgress  READ overallProgress  NOTIFY progressChanged)
    Q_PROPERTY(double       stageProgress    READ stageProgress    NOTIFY progressChanged)
    Q_PROPERTY(int          currentSegment   READ currentSegment   NOTIFY progressChanged)
    Q_PROPERTY(int          totalSegments    READ totalSegments    NOTIFY progressChanged)
    Q_PROPERTY(QString      videoPath        READ videoPath        WRITE setVideoPath NOTIFY videoPathChanged)
    Q_PROPERTY(QString      lastErrorMessage READ lastErrorMessage NOTIFY errorOccurred)
    Q_PROPERTY(bool         hasReport        READ hasReport        NOTIFY reportChanged)
    Q_PROPERTY(QString      model            READ model            WRITE setModel NOTIFY modelChanged)

public:
    explicit SummaryViewModel(QObject* parent = nullptr);
    ~SummaryViewModel() override;

    // ===== 可观察属性 getter =====
    SummaryState state() const           { return m_state; }
    double       overallProgress() const { return m_overall; }
    double       stageProgress() const   { return m_stageProgress; }
    int          currentSegment() const  { return m_currentSegment; }
    int          totalSegments() const   { return m_totalSegments; }
    QString      videoPath() const       { return m_videoPath; }
    QString      lastErrorMessage() const{ return m_lastError; }
    bool         hasReport() const       { return m_report.isValid; }
    QString      model() const           { return m_model; }

    // ===== 只读数据访问（View 在 reportChanged / 进度信号后拉取一次即可） =====
    const SummaryReport&         report()          const { return m_report; }
    int                          segmentCount()    const { return m_manager ? m_manager->segmentCount() : 0; }
    const SummarySegment*        segmentAt(int i)  const { return m_manager ? m_manager->segmentAt(i) : nullptr; }
    const QList<SubtitleItem>&   asrResults()      const;
    VideoSummaryManager::Progress progress()       const { return m_manager ? m_manager->progress() : VideoSummaryManager::Progress{}; }

    // ===== 缓存（透传 manager 的静态/实例方法） =====
    bool tryLoadFromCache(const QString& videoPath);
    void saveToCache();
    static void   clearAllCache()      { VideoSummaryManager::clearAllCache(); }
    static qint64 cacheTotalSize()     { return VideoSummaryManager::cacheTotalSize(); }
    static int    cacheFileCount()     { return VideoSummaryManager::cacheFileCount(); }

public slots:
    // ===== 命令 =====
    // 切视频；路径未变则不动，路径变化时自动 stop（若仍在进行）并重置 report
    void setVideoPath(const QString& path);
    void start();                                 // 等价 manager.startSummary(m_videoPath)
    void stop();
    void rerun();                                 // 强制重新分析（绕开缓存）
    void exportMarkdownTo(const QString& filePath); // VM 内部完成 Markdown 写盘
    void setModel(const QString& model);

signals:
    // ===== 属性变化通知 =====
    void stateChanged(SummaryState state);
    void progressChanged();                       // overall/stage/cur/total 任一变化
    void videoPathChanged(const QString& path);
    void errorOccurred(const QString& message);
    void reportChanged();                         // structuredReportReady 后整体更新
    void modelChanged(const QString& model);

    // ===== 业务事件 =====
    void segmentAnalyzed(int index, const QString& description);
    void asrCompleted(const QList<SubtitleItem>& items);
    void fullReportReady(const QString& reportJson);

    // ===== 用户意图（View 转发上来） =====
    void seekRequested(qint64 ms);

private slots:
    void onMgrStateChanged(SummaryState s);
    void onMgrSegmentAnalyzed(int index, const QString& description);
    void onMgrProgressUpdated(double progress);
    void onMgrProgressDetail(const VideoSummaryManager::Progress& p);
    void onMgrStructuredReportReady(const SummaryReport& report);
    void onMgrFullReportReady(const QString& reportJson);
    void onMgrAsrCompleted(const QList<SubtitleItem>& items);
    void onMgrError(const QString& msg);

private:
    VideoSummaryManager* m_manager = nullptr;
    QString              m_videoPath;
    QString              m_model;
    SummaryReport        m_report;
    QString              m_lastError;

    // 进度快照（独立于 manager，便于 Q_PROPERTY READ）
    SummaryState m_state           = SummaryState::Idle;
    double       m_overall         = 0.0;
    double       m_stageProgress   = 0.0;
    int          m_currentSegment  = 0;
    int          m_totalSegments   = 0;
};

#endif // SUMMARYVIEWMODEL_H
