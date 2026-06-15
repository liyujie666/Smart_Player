#ifndef TRANSCRIPTPANEL_H
#define TRANSCRIPTPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QHash>
#include <QSet>
#include <QString>
#include <QList>
#include <videosummarynetworkclient.h>

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

// 一行条目类型
enum RowType {
    Row_ChapterHeader,
    Row_Paragraph
};

// 字/词级时间戳（v3 逐字变蓝）
struct WordSegment {
    int startUtf16 = 0;
    int endUtf16   = 0;
    double startSec = 0.0;
    double endSec   = 0.0;
};

// 单条字幕的字/词缓存
struct SubtitleLineCache {
    int subtitleIndex = -1;
    QString text;
    QList<WordSegment> words;
};

// 内部行项
//  Row_ChapterHeader: 章节标题（可折叠）
//  Row_Paragraph    : 一个章节下的所有字幕拼成的一段话
//                     - m_subtitleIndices 记录包含的句子
//                     - m_firstSubtitleIndex 是 m_subtitleIndices[0]，
//                       保留旧字段以便 findRowIndexBySubtitle / 染色逻辑最小改动
struct RowItem {
    RowType type = Row_Paragraph;
    int chapterIndex = -1;
    int subtitleIndex = -1;        // 兼容旧：= firstSubtitleIndex
    int firstSubtitleIndex = -1;   // 段首句索引
    QList<int> subtitleIndices;    // 段内句索引
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString displayTime;           // 章节头: "00:00 - 01:23"
    QString displayText;           // 章节头: 标题；段落: 拼接文本缓存
    bool collapsed = false;
};

// 自定义委托，负责章节头/段落的精细绘制
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

// 视频文稿面板
class TranscriptPanel : public QWidget {
    Q_OBJECT
public:
    explicit TranscriptPanel(QWidget* parent = nullptr);
    ~TranscriptPanel() override;

    void setVideoPath(const QString& path);
    QString videoPath() const { return m_currentVideoPath; }

    // 暴露给 ItemDelegate
    const QList<RowItem>& rows() const { return m_rows; }
    int activeSubtitleIdx() const { return m_activeSubtitleIdx; }
    int activeChapterIdx() const { return m_activeChapterIdx; }
    qint64 currentPosMs() const { return m_currentPosMs; }
    bool wordLevelEnabled() const { return m_wordLevelEnabled; }
    bool isSearchHit(int subtitleIndex) const;
    int activeWordInLine(int subtitleIndex, qint64 posMs) const;

    // 供 ItemDelegate 渲染段落时查询字幕/字级缓存（不暴露给外部）
    const QList<SubtitleItem>& subtitles() const { return m_subtitles; }
    QHash<int, SubtitleLineCache*> lineCache() const { return m_lineCache; }
    int hoverSubtitleIdx() const { return m_hoverSubtitleIdx; }
    QString tooltipForSubtitle(int subtitleIndex) const;

    // 段落内句子的命中矩形（paint 时更新），用于悬浮 tooltip / 鼠标定位
    struct HitRect { int rowInList; int subtitleIndex; QRect rect; };
    const QList<HitRect>& hitRects() const { return m_hitRects; }
    void clearHitRects();
    void clearHitRectsForRow(int rowInList);
    void addHitRect(int rowInList, int subtitleIndex, const QRect& rect);

    // 鼠标位置 → 句子（章节头返回 -1）
    int subtitleAtViewportPos(const QPoint& vp) const;

    int listWidth() const;
    const QString& searchKeyword() const { return m_searchKeyword; }

signals:
    void seekTo(qint64 ms);

public slots:
    void onPositionChanged(qint64 positionMs);
    void setSubtitleItems(const QList<SubtitleItem>& items);
    void setChapters(const QList<SummaryChapter>& chapters);
    void setSegments(const QList<SummarySegment>& segments);
    void setDuration(qint64 ms);
    void clearAll();

    // 暴露给 ItemDelegate 调用（点击识别块头折叠按钮）
    bool handleHeaderClick(int rowInList, const QPoint& localPos, const QRect& itemRect);
    // 暴露给 ItemDelegate 调用（双击章节头 / 段落 → seek）
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

private:
    // 重建
    void rebuildRows();
    void rebuildListWidget();
    void applyCollapseToList();
    void scheduleRebuildIfReady();
    QString formatTime(qint64 ms) const;

    // 染色
    void applyHighlight(qint64 posMs);
    int findActiveSubtitleIndex(qint64 posMs) const;
    int findRowIndexBySubtitle(int subtitleIndex) const;
    int findRowIndexByChapter(int chapterIndex) const;
    int getBlockState(int chapterIndex) const;

    // 滚动
    void scrollToActiveRow();

    // 智能分词 + 字级时间戳（保留：K 歌需要）
    void ensureLineCache(int subtitleIndex);
    SubtitleLineCache buildLineCache(int subtitleIndex) const;
    static QList<QPair<int,int>> tokenize(const QString& text);
    static double computeTokenWeight(const QString& token);
    static bool isPunctuation(QChar c);

    // 状态
    QList<RowItem>                       m_rows;
    void seekFromRow(int row, const QPoint& viewportPos);
    QList<SubtitleItem>                  m_subtitles;
    QList<SummaryChapter>                m_chapters;
    QList<SummarySegment>                m_segments;
    QHash<int, SubtitleLineCache*>       m_lineCache;
    QSet<int>                            m_searchHitRows;     // 命中搜索的 subIdx 集合
    QList<HitRect>                       m_hitRects;          // paint 时填充
    QListWidget*                         m_list = nullptr;
    QLineEdit*                           m_search = nullptr;
    QPushButton*                         m_btnCollapseFinished = nullptr;
    QPushButton*                         m_btnExpandAll = nullptr;
    QPushButton*                         m_btnCollapseAll = nullptr;
    QCheckBox*                           m_chkWordLevel = nullptr;
    QTimer*                              m_throttleTimer = nullptr;
    QTimer*                              m_autoScrollResumeTimer = nullptr;
    QLabel*                              m_emptyLabel = nullptr;
    QString                              m_currentVideoPath;
    QString                              m_searchKeyword;
    qint64                               m_currentPosMs = -1;
    qint64                               m_totalDurationMs = 0;
    int                                  m_activeSubtitleIdx = -1;
    int                                  m_activeChapterIdx = -1;
    int                                  m_hoverSubtitleIdx = -1;  // 当前悬浮句子
    bool                                 m_hasChapters = false;
    bool                                 m_hasSubtitles = false;
    bool                                 m_hasSegments = false;
    bool                                 m_autoScroll = true;
    bool                                 m_wordLevelEnabled = true;
};

#endif // TRANSCRIPTPANEL_H
