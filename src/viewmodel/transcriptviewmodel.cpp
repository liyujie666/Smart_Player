#include "transcriptviewmodel.h"

#include <QStringList>
#include <algorithm>
#include <cmath>

// ============================================================
// 内部工具
// ============================================================
namespace {
// 句子拼接：句子间用"空格"隔开，去掉多余空行/收尾空白
QString joinSentences(const QStringList& lines) {
    QStringList cleaned;
    cleaned.reserve(lines.size());
    for (const QString& s : lines) {
        QString t = s.simplified();
        if (t.isEmpty()) continue;
        cleaned.append(t);
    }
    return cleaned.join(' ');
}
}

TranscriptViewModel::TranscriptViewModel(QObject* parent) : IViewModel(parent) {}

TranscriptViewModel::~TranscriptViewModel() {
    qDeleteAll(m_lineCache);
    m_lineCache.clear();
}

// ============================================================
// 简单属性
// ============================================================
QString TranscriptViewModel::formatTime(qint64 ms) const {
    if (ms < 0) ms = 0;
    int totalSec = int(ms / 1000);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    if (h > 0) return QString::asprintf("%d:%02d:%02d", h, m, s);
    return QString::asprintf("%02d:%02d", m, s);
}

bool TranscriptViewModel::isSearchHit(int subtitleIndex) const {
    return m_searchHitSubIdxs.contains(subtitleIndex);
}

bool TranscriptViewModel::isChapterCollapsed(int chapterIndex) const {
    return m_collapsedChapters.contains(chapterIndex);
}

QString TranscriptViewModel::tooltipForSubtitle(int subtitleIndex) const {
    if (subtitleIndex < 0 || subtitleIndex >= m_subtitles.size()) return QString();
    const SubtitleItem& sub = m_subtitles[subtitleIndex];
    qint64 startMs = qint64(std::llround(sub.start_sec * 1000.0));
    qint64 endMs   = qint64(std::llround(sub.end_sec   * 1000.0));
    return QStringLiteral(u"\u23f1 %1 \u2014 %2")
        .arg(formatTime(startMs), formatTime(endMs));
}

// ============================================================
// 数据 setter
// ============================================================
void TranscriptViewModel::setVideoPath(const QString& path) {
    if (path == m_videoPath) return;
    m_videoPath = path;
    clearAll();
    emit videoPathChanged(m_videoPath);
}

void TranscriptViewModel::setSubtitles(const QList<SubtitleItem>& items) {
    m_subtitles = items;
    bool changed = (m_hasSubtitles != !items.isEmpty());
    m_hasSubtitles = !items.isEmpty();
    qDeleteAll(m_lineCache);
    m_lineCache.clear();

    if (m_hasSubtitles) {
        rebuildRows();
        recomputeSearchHits();
        recomputeActiveIndices();
        emit rowsRebuilt();
    }
    if (changed) emit dataReady();
}

void TranscriptViewModel::setChapters(const QList<SummaryChapter>& chapters) {
    m_chapters = chapters;
    bool changed = (m_hasChapters != !chapters.isEmpty());
    m_hasChapters = !chapters.isEmpty();

    if (m_hasSubtitles) {
        rebuildRows();
        recomputeSearchHits();
        recomputeActiveIndices();
        emit rowsRebuilt();
    }
    if (changed) emit dataReady();
}

void TranscriptViewModel::setSegments(const QList<SummarySegment>& segments) {
    m_segments = segments;
    bool changed = (m_hasSegments != !segments.isEmpty());
    m_hasSegments = !segments.isEmpty();

    if (m_hasSubtitles) {
        rebuildRows();
        recomputeSearchHits();
        recomputeActiveIndices();
        emit rowsRebuilt();
    }
    if (changed) emit dataReady();
}

void TranscriptViewModel::setDuration(qint64 ms) {
    // 注意：原 TranscriptPanel 把传入的 ms × 1000 存为 μs（"m_totalDurationMs = ms * 1000"）。
    // 这里保持与原先一致：调用者传 ms，VM 内部存 μs。
    qint64 us = ms * 1000;
    if (m_durationMs == us) return;
    m_durationMs = us;
    emit durationChanged(m_durationMs);
}

void TranscriptViewModel::clearAll() {
    m_subtitles.clear();
    m_chapters.clear();
    m_segments.clear();
    m_rows.clear();
    qDeleteAll(m_lineCache);
    m_lineCache.clear();
    m_searchHitSubIdxs.clear();
    m_collapsedChapters.clear();
    m_searchKeyword.clear();
    m_currentPosMs = -1;
    m_activeSubtitleIdx = -1;
    m_activeChapterIdx = -1;
    m_durationMs = 0;
    m_hasChapters = m_hasSubtitles = m_hasSegments = false;
    m_autoScroll = true;

    emit dataReady();
    emit rowsRebuilt();
    emit activeIndexChanged();
    emit searchChanged(m_searchKeyword);
    emit autoScrollChanged(m_autoScroll);
}

// ============================================================
// 状态控制
// ============================================================
void TranscriptViewModel::updatePosition(qint64 ms) {
    // 原 TranscriptPanel::onPositionChanged 同样 *1000 转 μs
    qint64 us = ms * 1000;
    if (m_currentPosMs == us && m_activeSubtitleIdx != -1) {
        // 位置未变；不发位置信号（避免风暴），但 K 歌字级仍需要 View 重绘当前段，
        // 让 View 自己用 wordLevelEnabled() && activeSubtitleIdx() 决策。
        // 这里仅 emit positionChanged 让 View 知道"时间在走"，View 自行节流。
    }
    m_currentPosMs = us;
    emit positionChanged(m_currentPosMs);
    if (m_rows.isEmpty()) return;
    recomputeActiveIndices();
}

void TranscriptViewModel::setWordLevelEnabled(bool on) {
    if (m_wordLevelEnabled == on) return;
    m_wordLevelEnabled = on;
    emit wordLevelToggled(on);
}

void TranscriptViewModel::setSearchKeyword(const QString& kw) {
    if (m_searchKeyword == kw) return;
    m_searchKeyword = kw;
    recomputeSearchHits();
    emit searchChanged(m_searchKeyword);
}

void TranscriptViewModel::setAutoScroll(bool on) {
    if (m_autoScroll == on) return;
    m_autoScroll = on;
    emit autoScrollChanged(on);
}

// ============================================================
// 折叠
// ============================================================
void TranscriptViewModel::toggleChapterCollapsed(int chapterIndex) {
    if (chapterIndex < 0) return;
    if (m_collapsedChapters.contains(chapterIndex)) {
        m_collapsedChapters.remove(chapterIndex);
    } else {
        m_collapsedChapters.insert(chapterIndex);
    }
    // 同步到 rows 内 collapsed 字段（兼容 ItemDelegate 老用法）
    for (RowItem& r : m_rows) {
        if (r.type == RowType::ChapterHeader && r.chapterIndex == chapterIndex) {
            r.collapsed = m_collapsedChapters.contains(chapterIndex);
        }
    }
    emit rowsCollapseChanged();
}

void TranscriptViewModel::expandAll() {
    if (m_collapsedChapters.isEmpty()) return;
    m_collapsedChapters.clear();
    for (RowItem& r : m_rows) {
        if (r.type == RowType::ChapterHeader) r.collapsed = false;
    }
    emit rowsCollapseChanged();
}

void TranscriptViewModel::collapseAll() {
    bool changed = false;
    for (RowItem& r : m_rows) {
        if (r.type == RowType::ChapterHeader && !m_collapsedChapters.contains(r.chapterIndex)) {
            m_collapsedChapters.insert(r.chapterIndex);
            r.collapsed = true;
            changed = true;
        }
    }
    if (changed) emit rowsCollapseChanged();
}

void TranscriptViewModel::collapseAllFinishedChapters() {
    bool any = false;
    for (RowItem& r : m_rows) {
        if (r.type != RowType::ChapterHeader) continue;
        if (getBlockState(r.chapterIndex) == 2) {
            if (!m_collapsedChapters.contains(r.chapterIndex)) {
                m_collapsedChapters.insert(r.chapterIndex);
                any = true;
            }
            r.collapsed = true;
        }
    }
    if (any) emit rowsCollapseChanged();
}

// ============================================================
// 用户意图
// ============================================================
void TranscriptViewModel::requestSeekToMs(qint64 ms) {
    if (ms < 0) ms = 0;
    emit seekRequested(ms);
}

void TranscriptViewModel::requestSeekToSubtitle(int subtitleIndex) {
    if (subtitleIndex < 0 || subtitleIndex >= m_subtitles.size()) return;
    qint64 target = qint64(std::llround(m_subtitles[subtitleIndex].start_sec * 1000.0));
    emit seekRequested(target);
}

void TranscriptViewModel::requestSeekToChapter(int chapterIndex) {
    if (chapterIndex < 0) return;
    for (const RowItem& r : m_rows) {
        if (r.type == RowType::ChapterHeader && r.chapterIndex == chapterIndex) {
            emit seekRequested(r.startMs < 0 ? 0 : r.startMs);
            return;
        }
    }
}

// ============================================================
// rows 构建（从 TranscriptPanel::rebuildRows 平移）
// ============================================================
void TranscriptViewModel::rebuildRows() {
    m_rows.clear();

    struct Block { qint64 startMs, endMs; QString title; };
    QList<Block> blocks;

    if (!m_chapters.isEmpty()) {
        for (int i = 0; i < m_chapters.size(); ++i) {
            const auto& ch = m_chapters[i];
            QString title = ch.title.isEmpty()
                ? QStringLiteral(u"\u7b2c %1 \u6bb5").arg(i + 1)
                : ch.title;
            blocks.append({ch.startMs, ch.endMs, title});
        }
    } else if (!m_segments.isEmpty()) {
        for (int i = 0; i < m_segments.size(); ++i) {
            const auto& seg = m_segments[i];
            blocks.append({seg.startMs, seg.endMs,
                           QStringLiteral(u"\u7b2c %1 \u6bb5").arg(i + 1)});
        }
    } else if (!m_subtitles.isEmpty()) {
        blocks.append({0, m_durationMs, QStringLiteral(u"\u5168\u6587")});
    } else {
        return;
    }

    std::sort(blocks.begin(), blocks.end(), [](const Block& a, const Block& b) {
        return a.startMs < b.startMs;
    });

    for (int i = 0; i < blocks.size(); ++i) {
        if (blocks[i].endMs <= blocks[i].startMs) {
            blocks[i].endMs = blocks[i].startMs + 1;
        }
        if (i + 1 < blocks.size() && blocks[i].endMs > blocks[i + 1].startMs) {
            blocks[i].endMs = blocks[i + 1].startMs;
        }
    }

    for (int bi = 0; bi < blocks.size(); ++bi) {
        const Block& b = blocks[bi];

        // 收集属于本章节的字幕索引
        QList<int> subIndices;
        QStringList texts;
        qint64 blockStartMs = b.startMs, blockEndMs = b.endMs;
        for (int si = 0; si < m_subtitles.size(); ++si) {
            const auto& sub = m_subtitles[si];
            qint64 subStart = qint64(std::llround(sub.start_sec * 1000.0));
            qint64 subEnd   = qint64(std::llround(sub.end_sec * 1000.0));
            if (subEnd > b.startMs && subStart < b.endMs) {
                subIndices.append(si);
                texts.append(QString::fromStdString(sub.text));
            }
        }
        if (subIndices.isEmpty()) continue;

        // 章节头
        RowItem header;
        header.type = RowType::ChapterHeader;
        header.chapterIndex = bi;
        header.subtitleIndex = -1;
        header.firstSubtitleIndex = subIndices.first();
        header.subtitleIndices = subIndices;
        header.startMs = b.startMs;
        header.endMs = b.endMs;
        header.displayTime = QStringLiteral(u"%1 - %2")
            .arg(formatTime(b.startMs), formatTime(b.endMs));
        header.displayText = b.title;
        header.collapsed = m_collapsedChapters.contains(bi);
        m_rows.append(header);

        // 段落
        RowItem para;
        para.type = RowType::Paragraph;
        para.chapterIndex = bi;
        para.firstSubtitleIndex = subIndices.first();
        para.subtitleIndex = subIndices.first();
        para.subtitleIndices = subIndices;
        para.startMs = blockStartMs;
        para.endMs = blockEndMs;
        para.displayTime = header.displayTime;
        para.displayText = joinSentences(texts);
        para.collapsed = false;
        m_rows.append(para);

        // 预建字级缓存（K 歌用）
        for (int si : subIndices) ensureLineCache(si);
    }
}

// ============================================================
// 高亮位置查找
// ============================================================
void TranscriptViewModel::recomputeActiveIndices() {
    int oldSub = m_activeSubtitleIdx;
    int oldCh  = m_activeChapterIdx;

    int newSub = findActiveSubtitleIndex(m_currentPosMs);
    int newCh  = -1;
    if (newSub >= 0) {
        int rowIdx = findRowIndexBySubtitle(newSub);
        if (rowIdx >= 0) newCh = m_rows[rowIdx].chapterIndex;
    }

    if (newSub == oldSub && newCh == oldCh) return;

    m_activeSubtitleIdx = newSub;
    m_activeChapterIdx  = newCh;
    emit activeIndexChanged();
}

int TranscriptViewModel::findActiveSubtitleIndex(qint64 posMs) const {
    if (m_subtitles.isEmpty()) return -1;
    int lo = 0, hi = m_subtitles.size() - 1, ans = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        qint64 s = qint64(std::llround(m_subtitles[mid].start_sec * 1000.0));
        if (s <= posMs) { ans = mid; lo = mid + 1; }
        else            { hi = mid - 1; }
    }
    if (ans < 0) return -1;
    qint64 endMs = qint64(std::llround(m_subtitles[ans].end_sec * 1000.0));
    if (posMs <= endMs) return ans;
    if (ans + 1 < m_subtitles.size()) {
        qint64 nextStart = qint64(std::llround(m_subtitles[ans + 1].start_sec * 1000.0));
        if (posMs < nextStart) return ans;
        return ans;
    }
    return ans;
}

int TranscriptViewModel::findRowIndexBySubtitle(int subtitleIndex) const {
    if (subtitleIndex < 0) return -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        const RowItem& r = m_rows[i];
        if (r.type == RowType::Paragraph && r.subtitleIndices.contains(subtitleIndex)) return i;
        if (r.type == RowType::ChapterHeader && r.subtitleIndex == subtitleIndex) return i;
    }
    return -1;
}

int TranscriptViewModel::findRowIndexByChapter(int chapterIndex) const {
    if (chapterIndex < 0) return -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].type == RowType::ChapterHeader && m_rows[i].chapterIndex == chapterIndex) {
            return i;
        }
    }
    return -1;
}

int TranscriptViewModel::getBlockState(int chapterIndex) const {
    if (chapterIndex < 0) return -1;
    qint64 s = 0, e = 0;
    bool found = false;
    for (const RowItem& r : m_rows) {
        if (r.type == RowType::ChapterHeader && r.chapterIndex == chapterIndex) {
            s = r.startMs; e = r.endMs; found = true; break;
        }
    }
    if (!found) {
        if (chapterIndex >= 0 && chapterIndex < m_chapters.size()) {
            s = m_chapters[chapterIndex].startMs;
            e = m_chapters[chapterIndex].endMs;
        } else {
            return -1;
        }
    }
    if (m_currentPosMs < s) return 0;
    if (m_currentPosMs >= e) return 2;
    return 1;
}

// ============================================================
// 搜索
// ============================================================
void TranscriptViewModel::recomputeSearchHits() {
    m_searchHitSubIdxs.clear();
    if (m_searchKeyword.isEmpty()) return;
    for (int si = 0; si < m_subtitles.size(); ++si) {
        if (QString::fromStdString(m_subtitles[si].text)
                .contains(m_searchKeyword, Qt::CaseInsensitive)) {
            m_searchHitSubIdxs.insert(si);
        }
    }
}

// ============================================================
// 字级时间戳（线性插值）
// ============================================================
int TranscriptViewModel::activeWordInLine(int subtitleIndex, qint64 posMs) const {
    auto it = m_lineCache.constFind(subtitleIndex);
    if (it == m_lineCache.constEnd()) return -1;
    const SubtitleLineCache* cache = it.value();
    if (!cache) return -1;
    const QList<WordSegment>& words = cache->words;
    if (words.isEmpty()) return -1;

    double posSec = posMs / 1000.0;
    int lo = 0, hi = words.size() - 1, ans = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (words[mid].startSec <= posSec) { ans = mid; lo = mid + 1; }
        else { hi = mid - 1; }
    }
    if (ans < 0) return -1;
    if (posSec < words[ans].endSec) return ans;
    if (ans + 1 < words.size()) return ans + 1;
    return words.size() - 1;
}

void TranscriptViewModel::ensureLineCache(int subtitleIndex) {
    if (subtitleIndex < 0 || subtitleIndex >= m_subtitles.size()) return;
    if (m_lineCache.contains(subtitleIndex)) return;
    SubtitleLineCache* cache = new SubtitleLineCache(buildLineCache(subtitleIndex));
    m_lineCache.insert(subtitleIndex, cache);
}

SubtitleLineCache TranscriptViewModel::buildLineCache(int subtitleIndex) const {
    SubtitleLineCache cache;
    if (subtitleIndex < 0 || subtitleIndex >= m_subtitles.size()) return cache;
    const auto& sub = m_subtitles[subtitleIndex];
    cache.subtitleIndex = subtitleIndex;
    cache.text = QString::fromStdString(sub.text).simplified();
    if (cache.text.isEmpty()) return cache;

    QList<QPair<int,int>> tokens = tokenize(cache.text);
    if (tokens.isEmpty()) return cache;

    QList<double> weights;
    double totalWeight = 0;
    for (const auto& tok : tokens) {
        QString s = cache.text.mid(tok.first, tok.second - tok.first);
        double w = computeTokenWeight(s);
        if (w <= 0) w = 0.0001;
        weights.append(w);
        totalWeight += w;
    }

    double duration = sub.end_sec - sub.start_sec;
    if (duration <= 0) duration = 0.001;
    double cursor = sub.start_sec;
    for (int i = 0; i < tokens.size(); ++i) {
        double dt = (weights[i] / totalWeight) * duration;
        WordSegment ws;
        ws.startUtf16 = tokens[i].first;
        ws.endUtf16   = tokens[i].second;
        ws.startSec   = cursor;
        ws.endSec     = cursor + dt;
        cache.words.append(ws);
        cursor = ws.endSec;
    }
    return cache;
}

bool TranscriptViewModel::isPunctuation(QChar c) {
    ushort u = c.unicode();
    if (u == 0xFF0C || u == 0x3002 || u == 0xFF01 || u == 0xFF1F ||
        u == 0xFF1A || u == 0xFF1B || u == 0x3001 || u == 0x3003 ||
        u == 0x201C || u == 0x201D || u == 0x2018 || u == 0x2019 ||
        u == 0xFF08 || u == 0xFF09 || u == 0x3010 || u == 0x3011 ||
        u == 0x2026 || u == 0x00B7) {
        return true;
    }
    if (u < 128) {
        return !(c.isLetterOrNumber()) && !c.isSpace();
    }
    if ((u >= 0x3000 && u <= 0x303F) ||
        (u >= 0xFF00 && u <= 0xFFEF) ||
        (u >= 0x2000 && u <= 0x206F)) {
        return true;
    }
    return false;
}

QList<QPair<int,int>> TranscriptViewModel::tokenize(const QString& text) {
    QList<QPair<int,int>> tokens;
    int n = text.size();
    int i = 0;
    while (i < n) {
        QChar c = text[i];
        if (c.isSpace()) { ++i; continue; }
        if (isPunctuation(c)) {
            tokens.append({i, i + 1});
            ++i; continue;
        }
        ushort u = c.unicode();
        if (u >= 0x4E00 && u <= 0x9FFF) {
            tokens.append({i, i + 1});
            ++i; continue;
        }
        if ((u >= 0x3400 && u <= 0x4DBF) ||
            (u >= 0x20000 && u <= 0x2A6DF) ||
            (u >= 0x2A700 && u <= 0x2B73F) ||
            (u >= 0x2B740 && u <= 0x2B81F) ||
            (u >= 0x2B820 && u <= 0x2CEAF)) {
            tokens.append({i, i + 1});
            ++i; continue;
        }
        if (c.isLetterOrNumber() && u < 128) {
            int j = i;
            while (j < n) {
                QChar cj = text[j];
                ushort uj = cj.unicode();
                if (cj.isLetterOrNumber() && uj < 128) ++j;
                else break;
            }
            if (j > i) {
                tokens.append({i, j});
                i = j;
                continue;
            }
        }
        tokens.append({i, i + 1});
        ++i;
    }
    return tokens;
}

double TranscriptViewModel::computeTokenWeight(const QString& token) {
    if (token.isEmpty()) return 0.0;
    if (isPunctuation(token.at(0))) return 0.1;
    ushort u = token.at(0).unicode();
    if (token.size() == 1 && u >= 0x4E00 && u <= 0x9FFF) return 1.0;
    if (token.at(0).isLetterOrNumber()) return double(token.size()) * 1.2;
    return 1.0;
}
