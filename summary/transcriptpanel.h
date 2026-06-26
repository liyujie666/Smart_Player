#ifndef TRANSCRIPTPANEL_H
#define TRANSCRIPTPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QHash>
#include <QSet>
#include <QString>
#include <QList>
#include "viewmodel/transcriptviewmodel.h"   // 提供 RowType / RowItem / WordSegment / SubtitleLineCache
#include "summary/videosummarynetworkclient.h"

class QLineEdit;
class QPushButton;
class QCheckBox;
class QTimer;
class QLabel;
class QPainter;
class QStyleOptionViewItem;
class QModelIndex;
class QEvent;
class QMouseEvent;
class QPoint;
class QRect;
class TranscriptPanel;

// 自定义委托，负责章节头/段落的精细绘制（数据通过 panel->vm() 取）
class TranscriptItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit TranscriptItemDelegate(TranscriptPanel* panel, QObject* parent = nullptr);

    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& idx) const override;
    QSize sizeHint(const QStyleOptionViewItem& o, const QModelIndex& idx) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option, const QModelIndex& index) override;

private:
    void paintChapterHeader(QPainter* p, const QStyleOptionViewItem& o, const RowItem& row) const;
    void paintParagraph(QPainter* p, const QStyleOptionViewItem& o, const RowItem& row,
                        int rowInList) const;

    TranscriptPanel* m_panel = nullptr;
};

// 视频文稿面板（View 层）
//
// MVVM 阶段 3b：业务数据 / 计算 / 状态机已迁移到 TranscriptViewModel。
// TranscriptPanel 只负责：
//   - QListWidget 渲染 + 自定义 ItemDelegate
//   - 用户事件转发到 VM（搜索 / 折叠 / seek / 字级开关 / 自动滚动）
//   - 渲染响应 VM 的 signal（rowsRebuilt / rowsCollapseChanged / activeIndexChanged / searchChanged / ...）
class TranscriptPanel : public QWidget {
    Q_OBJECT
public:
    explicit TranscriptPanel(QWidget* parent = nullptr);
    ~TranscriptPanel() override;

    // MVVM 阶段 3b：通过 ViewModel 注入业务数据/状态
    void bindViewModel(TranscriptViewModel* vm);
    TranscriptViewModel* vm() const { return m_vm; }

    void setVideoPath(const QString& path);
    QString videoPath() const { return m_vm ? m_vm->videoPath() : QString(); }

    // 暴露给 ItemDelegate（透传到 VM）
    const QList<RowItem>& rows() const;
    int activeSubtitleIdx() const;
    int activeChapterIdx() const;
    qint64 currentPosMs() const;
    bool wordLevelEnabled() const;
    bool isSearchHit(int subtitleIndex) const;
    int activeWordInLine(int subtitleIndex, qint64 posMs) const;
    const QList<SubtitleItem>& subtitles() const;
    QHash<int, SubtitleLineCache*> lineCache() const;
    int hoverSubtitleIdx() const { return m_hoverSubtitleIdx; }
    QString tooltipForSubtitle(int subtitleIndex) const;

    // 段落内句子的命中矩形（paint 时更新，纯 UI 状态）
    struct HitRect { int rowInList; int subtitleIndex; QRect rect; };
    const QList<HitRect>& hitRects() const { return m_hitRects; }
    void clearHitRects();
    void clearHitRectsForRow(int rowInList);
    void addHitRect(int rowInList, int subtitleIndex, const QRect& rect);

    int subtitleAtViewportPos(const QPoint& vp) const;

    int listWidth() const;
    QString searchKeyword() const;

signals:
    // 兼容旧 API：MainWindow 仍 connect 此信号；内部由 VM::seekRequested 转发而来。
    void seekTo(qint64 ms);

public slots:
    // 透传到 VM 的便捷 slot（MainWindow 在 player.timeChanged / initFinished
    // / SummaryViewModel.asrCompleted 时调用这些 slot，VM 收到后处理）
    void onPositionChanged(qint64 positionMs);
    void setSubtitleItems(const QList<SubtitleItem>& items);
    void setChapters(const QList<SummaryChapter>& chapters);
    void setSegments(const QList<SummarySegment>& segments);
    void setDuration(qint64 ms);
    void clearAll();

    // 暴露给 ItemDelegate
    bool handleHeaderClick(int rowInList, const QPoint& localPos, const QRect& itemRect);
    void seekFromRowAt(int row, const QPoint& viewportPos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);
    void onCollapseFinishedClicked();
    void onExpandAllClicked();
    void onCollapseAllClicked();
    void onSearchTextChanged(const QString& kw);
    void onWordLevelToggled(bool on);
    void onScrollChanged();
    void onThrottleTimeout();
    void onAutoScrollResume();

    // VM → View
    void onVmRowsRebuilt();
    void onVmRowsCollapseChanged();
    void onVmActiveIndexChanged();
    void onVmSearchChanged(const QString& kw);
    void onVmDataReady();

private:
    void rebuildListWidget();
    void applyCollapseToList();
    void seekFromRow(int row, const QPoint& viewportPos);
    void scrollToActiveRow();

    // UI 状态
    TranscriptViewModel*                 m_vm = nullptr;

    QListWidget*                         m_list = nullptr;
    QLineEdit*                           m_search = nullptr;
    QPushButton*                         m_btnCollapseFinished = nullptr;
    QPushButton*                         m_btnExpandAll = nullptr;
    QPushButton*                         m_btnCollapseAll = nullptr;
    QCheckBox*                           m_chkWordLevel = nullptr;
    QTimer*                              m_throttleTimer = nullptr;
    QTimer*                              m_autoScrollResumeTimer = nullptr;
    QLabel*                              m_emptyLabel = nullptr;

    int                                  m_hoverSubtitleIdx = -1;
    QList<HitRect>                       m_hitRects;
    qint64                               m_pendingPositionMs = -1;   // 节流缓存：等 throttle timer 触发后再喂给 VM
};

#endif // TRANSCRIPTPANEL_H
