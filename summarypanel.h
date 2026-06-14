#ifndef SUMMARY_PANEL_H
#define SUMMARY_PANEL_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTextEdit>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QCheckBox>
#include <QSignalMapper>
#include <QToolButton>
#include "flowlayout.h"
#include "videosummarymanager.h"

class SummaryPanel : public QWidget {
    Q_OBJECT
public:
    explicit SummaryPanel(QWidget* parent = nullptr);
    void bindManager(VideoSummaryManager* mgr);

    // 设置当前视频路径：路径变化时自动重置面板到初始态，
    // 路径未变时不动（避免拖动进度条等场景误清空）。
    void setVideoPath(const QString& path);
    QString videoPath() const { return m_currentVideoPath; }

signals:
    void seekTo(qint64 ms);

public slots:
    void onPositionChanged(qint64 ms);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onRerunClicked();
    void onExportClicked();
    void onSettingsClicked();
    void onStateChanged(SummaryState state);
    void onSegmentAnalyzed(int index, const QString& desc);
    void onProgressUpdated(double progress);
    void onProgressDetailChanged(const VideoSummaryManager::Progress& progress);
    void onFullReportReady(const QString& reportJson);
    void onStructuredReportReady(const SummaryReport& report);
    void onErrorOccurred(const QString& message);
    void onSegmentClicked(QListWidgetItem* item);
    void onEntityClicked(const QString& entityName, qint64 ms);

private:
    void buildUI();
    void resetPanelForNewVideo();   // 切换视频时重置面板到初始占位态
    void updateSegmentItem(int index, const QString& desc, bool isAnalyzed);
    void updateStatusLabel(const VideoSummaryManager::Progress& progress);
    void rebuildSegmentList();
    void rebuildSegmentList(const QList<SummaryChapter>& chapters);
    void populateFromReport(const SummaryReport& report);
    void exportMarkdown(const SummaryReport& report);
    int findSegmentAtMs(qint64 ms) const;
    void highlightCurrentSegment(qint64 ms);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    VideoSummaryManager* m_manager = nullptr;

    // Toolbar
    QComboBox* m_cmbModel = nullptr;
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnStop = nullptr;
    QPushButton* m_btnRerun = nullptr;
    QPushButton* m_btnExport = nullptr;
    QPushButton* m_btnSettings = nullptr;

    // Meta bar
    QLabel* m_lblMeta = nullptr;

    // TL;DR
    QLabel* m_lblTldlr = nullptr;

    // Key Takeaways
    QWidget* m_keyTakeawaysWidget = nullptr;
    QVBoxLayout* m_keyTakeawaysLayout = nullptr;

    // Chapter Timeline
    QLabel* m_chapterHeader = nullptr;
    QListWidget* m_segmentList = nullptr;

    // 实体：默认展开，FlowLayout 排布的标签云
    QWidget* m_entitiesWidget = nullptr;
    QLabel* m_entitiesTitle = nullptr;
    QWidget* m_entitiesContent = nullptr;
    FlowLayout* m_entitiesContentLayout = nullptr;

    // Progress area
    QLabel* m_lblStatus = nullptr;
    QProgressBar* m_progressBar = nullptr;

    // Current playback position tracking
    qint64 m_currentPositionMs = -1;
    int m_highlightedSegment = -1;

    // 分析期间屏蔽章节列表实时刷新，等 structuredReportReady 统一展示
    bool m_analysisInProgress = false;

    QString m_currentVideoPath;
};

#endif // SUMMARY_PANEL_H
