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
#include "videosummarymanager.h"

class SummaryPanel : public QWidget {
    Q_OBJECT
public:
    explicit SummaryPanel(QWidget* parent = nullptr);
    void bindManager(VideoSummaryManager* mgr);
    void setVideoPath(const QString& path) { m_currentVideoPath = path; }
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
    void onStateChanged(SummaryState state);
    void onSegmentAnalyzed(int index, const QString& desc);
    void onProgressUpdated(double progress);
    void onProgressDetailChanged(const VideoSummaryManager::Progress& progress);
    void onFullReportReady(const QString& reportJson);
    void onStructuredReportReady(const SummaryReport& report);
    void onErrorOccurred(const QString& message);
    void onSegmentClicked(QListWidgetItem* item);
    void onEntityClicked(const QString& entityName, qint64 ms);
    void onTranscriptSearchChanged(const QString& text);

private:
    void buildUI();
    void updateSegmentItem(int index, const QString& desc, bool isAnalyzed);
    void updateStatusLabel(const VideoSummaryManager::Progress& progress);
    void rebuildSegmentList();
    void populateFromReport(const SummaryReport& report);
    void exportMarkdown(const SummaryReport& report);
    int findSegmentAtMs(qint64 ms) const;
    void highlightCurrentSegment(qint64 ms);
    void highlightTranscriptMatch(const QString& keyword);

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

    // Collapsible: Entities
    QWidget* m_entitiesWidget = nullptr;
    QWidget* m_entitiesHeader = nullptr;
    bool m_entitiesExpanded = false;
    QWidget* m_entitiesContent = nullptr;
    QVBoxLayout* m_entitiesContentLayout = nullptr;

    // Collapsible: Transcript
    QWidget* m_transcriptWidget = nullptr;
    QWidget* m_transcriptHeader = nullptr;
    bool m_transcriptExpanded = false;
    QWidget* m_transcriptContent = nullptr;
    QLineEdit* m_lineTranscriptSearch = nullptr;
    QTextEdit* m_txtTranscript = nullptr;

    // Progress area
    QLabel* m_lblStatus = nullptr;
    QProgressBar* m_progressBar = nullptr;

    // Current playback position tracking
    qint64 m_currentPositionMs = -1;
    int m_highlightedSegment = -1;

    QString m_currentVideoPath;
};

#endif // SUMMARY_PANEL_H
