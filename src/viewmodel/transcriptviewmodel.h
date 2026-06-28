#ifndef TRANSCRIPTVIEWMODEL_H
#define TRANSCRIPTVIEWMODEL_H

#include "iviewmodel.h"
#include "summary/videosummarynetworkclient.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>

// ============================================================
// 共享数据结构（与 TranscriptPanel 旧定义保持二进制兼容，
// 由 TranscriptPanel 的 ItemDelegate 直接消费）
// ============================================================

// 一行条目类型
enum class RowType : int {
    ChapterHeader,
    Paragraph
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
struct RowItem {
    RowType type = RowType::Paragraph;
    int chapterIndex = -1;
    int subtitleIndex = -1;        // 兼容旧：= firstSubtitleIndex
    int firstSubtitleIndex = -1;   // 段首句索引
    QList<int> subtitleIndices;    // 段内句索引
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString displayTime;           // 章节头: "00:00 - 01:23"
    QString displayText;           // 章节头: 标题；段落: 拼接文本缓存
    bool collapsed = false;        // 仅章节头使用；权威值在 VM 的 m_collapsedChapters
};

/**
 *  TranscriptViewModel
 *
 *  视频文稿面板的业务数据 / 计算 / 状态机层。
 *
 *  状态：
 *    - rows / subtitles / chapters / segments  ：原始数据 + 派生 rows
 *    - lineCache                                ：每句的字级时间戳缓存（K 歌用）
 *    - activeSubtitleIdx / activeChapterIdx    ：跟随播放位置的高亮态
 *    - currentPosMs                             ：当前播放位置（μs，与 PlayerCore::seek 一致）
 *    - durationMs                               ：视频总时长（μs）
 *    - searchKeyword / searchHitRows           ：搜索过滤
 *    - wordLevelEnabled / autoScroll           ：用户偏好
 *    - collapsedChapters                        ：业务折叠状态（每章独立）
 *
 *  Q_PROPERTY + signal 命名约定见 viewmodel/README.md。
 *
 *  与 View 的契约：
 *    - 数据 setter (setSubtitles/Chapters/Segments) → emit dataReady + rowsRebuilt
 *    - 折叠 (toggleChapterCollapsed/expandAll/collapseAll) → emit rowsCollapseChanged
 *    - 位置更新 (updatePosition) → 内部计算 activeXxx；不变化时静默；变化时 emit activeIndexChanged
 *    - 搜索 (setSearchKeyword) → 内部重算 m_searchHitRows → emit searchChanged
 *
 *  跨 VM：不直接耦合任何其它 VM；用 seekRequested 信号让 MainWindow 转发。
 */
class TranscriptViewModel : public IViewModel {
    Q_OBJECT
    Q_PROPERTY(QString videoPath         READ videoPath         WRITE setVideoPath NOTIFY videoPathChanged)
    Q_PROPERTY(qint64  durationMs        READ durationMs        WRITE setDuration  NOTIFY durationChanged)
    Q_PROPERTY(qint64  currentPositionMs READ currentPositionMs                    NOTIFY positionChanged)
    Q_PROPERTY(int     activeSubtitleIdx READ activeSubtitleIdx                    NOTIFY activeIndexChanged)
    Q_PROPERTY(int     activeChapterIdx  READ activeChapterIdx                     NOTIFY activeIndexChanged)
    Q_PROPERTY(bool    wordLevelEnabled  READ wordLevelEnabled  WRITE setWordLevelEnabled NOTIFY wordLevelToggled)
    Q_PROPERTY(QString searchKeyword     READ searchKeyword     WRITE setSearchKeyword    NOTIFY searchChanged)
    Q_PROPERTY(bool    autoScroll        READ autoScroll        WRITE setAutoScroll       NOTIFY autoScrollChanged)
    Q_PROPERTY(bool    hasChapters       READ hasChapters       NOTIFY dataReady)
    Q_PROPERTY(bool    hasSubtitles      READ hasSubtitles      NOTIFY dataReady)
    Q_PROPERTY(bool    hasSegments       READ hasSegments       NOTIFY dataReady)

public:
    explicit TranscriptViewModel(QObject* parent = nullptr);
    ~TranscriptViewModel() override;

    // ===== 只读属性 =====
    QString videoPath()         const { return m_videoPath; }
    qint64  durationMs()        const { return m_durationMs; }
    qint64  currentPositionMs() const { return m_currentPosMs; }
    int     activeSubtitleIdx() const { return m_activeSubtitleIdx; }
    int     activeChapterIdx()  const { return m_activeChapterIdx; }
    bool    wordLevelEnabled()  const { return m_wordLevelEnabled; }
    QString searchKeyword()     const { return m_searchKeyword; }
    bool    autoScroll()        const { return m_autoScroll; }
    bool    hasChapters()       const { return m_hasChapters; }
    bool    hasSubtitles()      const { return m_hasSubtitles; }
    bool    hasSegments()       const { return m_hasSegments; }

    // ===== 数据访问 =====
    const QList<RowItem>&                   rows()       const { return m_rows; }
    const QList<SubtitleItem>&              subtitles()  const { return m_subtitles; }
    const QList<SummaryChapter>&            chapters()   const { return m_chapters; }
    const QList<SummarySegment>&            segments()   const { return m_segments; }
    const QHash<int, SubtitleLineCache*>&   lineCache()  const { return m_lineCache; }

    // 查询
    bool    isSearchHit(int subtitleIndex) const;
    bool    isChapterCollapsed(int chapterIndex) const;
    int     activeWordInLine(int subtitleIndex, qint64 posMs) const;
    QString tooltipForSubtitle(int subtitleIndex) const;
    QString formatTime(qint64 ms) const;
    int     findActiveSubtitleIndex(qint64 posMs) const;
    int     findRowIndexBySubtitle(int subtitleIndex) const;
    int     findRowIndexByChapter(int chapterIndex) const;
    int     getBlockState(int chapterIndex) const;   // 0=未播, 1=正在播, 2=已播完

public slots:
    // ===== 数据注入命令 =====
    void setVideoPath(const QString& path);
    void setSubtitles(const QList<SubtitleItem>& items);
    void setChapters(const QList<SummaryChapter>& chapters);
    void setSegments(const QList<SummarySegment>& segments);
    void setDuration(qint64 ms);
    void clearAll();

    // ===== 状态控制命令 =====
    void updatePosition(qint64 ms);                  // 单位 μs（与 PlayerCore 一致）
    void setWordLevelEnabled(bool on);
    void setSearchKeyword(const QString& kw);
    void setAutoScroll(bool on);

    // ===== 折叠命令 =====
    void toggleChapterCollapsed(int chapterIndex);
    void expandAll();
    void collapseAll();
    void collapseAllFinishedChapters();              // 等价旧 onCollapseFinishedClicked

    // ===== 用户意图（View 转发 → VM emit seekRequested → MainWindow 转 player.seek） =====
    void requestSeekToMs(qint64 ms);
    void requestSeekToSubtitle(int subtitleIndex);
    void requestSeekToChapter(int chapterIndex);

signals:
    // 状态属性
    void videoPathChanged(const QString& p);
    void durationChanged(qint64 ms);
    void positionChanged(qint64 ms);
    void activeIndexChanged();                       // subtitle/chapter 任一变化
    void wordLevelToggled(bool on);
    void searchChanged(const QString& kw);
    void autoScrollChanged(bool on);

    // 结构变化
    void rowsRebuilt();                              // m_rows 重建完成，View 重新绑定到 QListWidget
    void rowsCollapseChanged();                      // 折叠状态变化，View 调 applyCollapseToList
    void dataReady();                                // hasChapters / Subtitles / Segments 任一变化

    // 用户意图
    void seekRequested(qint64 ms);

private:
    void rebuildRows();
    void ensureLineCache(int subtitleIndex);
    SubtitleLineCache buildLineCache(int subtitleIndex) const;
    static QList<QPair<int,int>> tokenize(const QString& text);
    static double computeTokenWeight(const QString& token);
    static bool   isPunctuation(QChar c);

    void recomputeSearchHits();
    void recomputeActiveIndices();                   // 由 updatePosition 内部调用

private:
    QList<RowItem>                  m_rows;
    QList<SubtitleItem>             m_subtitles;
    QList<SummaryChapter>           m_chapters;
    QList<SummarySegment>           m_segments;
    QHash<int, SubtitleLineCache*>  m_lineCache;
    QSet<int>                       m_searchHitSubIdxs;   // 命中搜索的句索引（注意：旧 panel 字段叫 m_searchHitRows，
                                                         //                语义同为 subIdx 集合，这里改名更直观）
    QSet<int>                       m_collapsedChapters;  // 折叠中的 chapter idx

    QString m_videoPath;
    QString m_searchKeyword;
    qint64  m_durationMs        = 0;
    qint64  m_currentPosMs      = -1;
    int     m_activeSubtitleIdx = -1;
    int     m_activeChapterIdx  = -1;
    bool    m_wordLevelEnabled  = true;
    bool    m_autoScroll        = true;

    bool m_hasChapters  = false;
    bool m_hasSubtitles = false;
    bool m_hasSegments  = false;
};

#endif // TRANSCRIPTVIEWMODEL_H
