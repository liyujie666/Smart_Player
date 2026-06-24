#include "transcriptpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QScrollBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QSet>
#include <QFontMetrics>
#include <QLabel>
#include <QSizePolicy>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QStyle>
#include <QTextLayout>
#include <QTextLine>
#include <QToolTip>
#include <QHelpEvent>
#include <QAbstractItemView>
#include <QDebug>
#include <algorithm>
#include <cmath>

// =============================================================
// 内部工具
// =============================================================
namespace {

static const QColor kHeaderBg       = QColor("#F9FAFB");
static const QColor kHeaderUnread   = QColor("#1F2937");
static const QColor kHeaderFinished = QColor("#94A3B8");
static const QColor kHeaderCurrent  = QColor("#0C4A6E");
static const QColor kAccentBlue     = QColor("#0EA5E9");
static const QColor kActiveBg       = QColor("#F0F9FF");
static const QColor kSubUnread      = QColor("#1F2937");
static const QColor kSubRead        = QColor("#0EA5E9");
static const QColor kTimeGray       = QColor("#6B7280");
static const QColor kHoverBg        = QColor("#F3F4F6");
static const QColor kKaraokeBlue    = QColor("#0EA5E9");

// 段落/章节头参数
static const int kParaPadLeft      = 8;
static const int kParaPadRight     = 10;
static const int kParaPadTop       = 6;
static const int kParaPadBottom    = 6;
static const int kParaLineSpacing  = 2;

// 句子拼接：句子间用"空格"隔开，去掉多余空行/收尾空白
static QString joinSentences(const QStringList& lines) {
    QStringList cleaned;
    cleaned.reserve(lines.size());
    for (const QString& s : lines) {
        QString t = s;
        // 折叠内部空白
        t = t.simplified();
        if (t.isEmpty()) continue;
        cleaned.append(t);
    }
    return cleaned.join(' ');
}

} // namespace

// =============================================================
// TranscriptItemDelegate
// =============================================================
TranscriptItemDelegate::TranscriptItemDelegate(TranscriptPanel* panel, QObject* parent)
    : QStyledItemDelegate(parent), m_panel(panel) {}

void TranscriptItemDelegate::paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& idx) const {
    if (!m_panel || !idx.isValid()) {
        QStyledItemDelegate::paint(p, o, idx);
        return;
    }
    const QList<RowItem>& rows = m_panel->rows();
    if (idx.row() < 0 || idx.row() >= rows.size()) return;
    const RowItem& row = rows[idx.row()];

    if (row.type == RowType::ChapterHeader) {
        paintChapterHeader(p, o, row);
    } else {
        paintParagraph(p, o, row, idx.row());
    }
}

QSize TranscriptItemDelegate::sizeHint(const QStyleOptionViewItem& o, const QModelIndex& idx) const {
    if (!m_panel || !idx.isValid()) return QStyledItemDelegate::sizeHint(o, idx);
    const QList<RowItem>& rows = m_panel->rows();
    if (idx.row() < 0 || idx.row() >= rows.size()) return QStyledItemDelegate::sizeHint(o, idx);
    const RowItem& row = rows[idx.row()];

    int w = o.rect.width();
    if (w <= 0 && m_panel) w = m_panel->listWidth();

    if (row.type == RowType::ChapterHeader) {
        return QSize(w, 26);
    }
    // Paragraph：用 QTextLayout 预计算高度
    QFont textFont;
    textFont.setFamily(QStringLiteral("Microsoft YaHei"));
    textFont.setPointSize(9);
    QFontMetrics fm(textFont);

    int textWidth = w - kParaPadLeft - kParaPadRight;

    // 关键：viewport 首次 layout 完成前 listWidth() 可能为 0；
    // 这种情况下不能用 textWidth=10 喂 QTextLayout（会把段落折成几十行），
    // 而是给一个保守的"未折行"高度，等 viewport resize 触发后 view 再重算。
    // 另外即使 textWidth 偏小（比如 < 60），textWidth 与真实宽度的偏差太大，估算没意义。
    if (textWidth < 60) {
        // 保守给 1 行 + 上下 padding（实际尺寸会在 viewport 有真实宽度时通过
        // scheduleSizeHintRefresh() 主动 doItemsLayout 重算）
        return QSize(w, kParaPadTop + fm.height() + kParaPadBottom);
    }

    QTextLayout layout(row.displayText, textFont);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opt);
    layout.beginLayout();
    int lineCount = 0;
    while (true) {
        QTextLine tl = layout.createLine();
        if (!tl.isValid()) break;
        tl.setLineWidth(textWidth);
        ++lineCount;
    }
    layout.endLayout();
    if (lineCount == 0) lineCount = 1;
    int h = kParaPadTop + lineCount * fm.height() + (lineCount - 1) * kParaLineSpacing + kParaPadBottom;
    return QSize(w, h);
}

bool TranscriptItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                         const QStyleOptionViewItem& option, const QModelIndex& index) {
    Q_UNUSED(model);
    if (!m_panel || !index.isValid()) {
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        QPoint local = me->pos() - option.rect.topLeft();
        // 章节头：单击只折叠 / 展开，seek 走双击
        return m_panel->handleHeaderClick(index.row(), local, option.rect);
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        // 章节头：双击 seek 到章节开始（QListWidget 也会发 itemDoubleClicked，
        // 但我们这里通过 editorEvent 提前拦截，确保语义统一）
        const QList<RowItem>& rows = m_panel->rows();
        if (index.row() < rows.size() && rows[index.row()].type == RowType::ChapterHeader) {
            QPoint vp = static_cast<QMouseEvent*>(event)->pos();
            m_panel->seekFromRowAt(index.row(), vp);
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void TranscriptItemDelegate::paintChapterHeader(QPainter* p, const QStyleOptionViewItem& o, const RowItem& row) const {
    QRect r = o.rect;
    p->save();
    p->fillRect(r, kHeaderBg);

    bool isCurrent = (m_panel->activeChapterIdx() == row.chapterIndex);
    if (isCurrent) {
        p->fillRect(r.left(), r.top(), 3, r.height(), kAccentBlue);
    }

    int left = r.left() + (isCurrent ? 8 + 3 : 8);
    int top = r.top();

    // 折叠/展开图标（PNG）
    static const QPixmap s_arrowDown  = QPixmap(QStringLiteral(":/SmartPlayer-icon/arrow_down_dark.png"));
    static const QPixmap s_arrowRight = QPixmap(QStringLiteral(":/SmartPlayer-icon/arrow_right_dark.png"));
    const QPixmap& arrow = row.collapsed ? s_arrowRight : s_arrowDown;
    int iconSize = 12;  // 章节头只有 26px 高，12 适配
    int iconY = r.top() + (r.height() - iconSize) / 2;
    if (!arrow.isNull()) {
        p->drawPixmap(left, iconY, iconSize, iconSize, arrow);
    } else {
        // 资源加载失败兜底：用字符箭头
        p->setPen(kHeaderUnread);
        QFont iconFont = p->font();
        iconFont.setPointSize(7);
        p->setFont(iconFont);
        QString ch = row.collapsed ? QStringLiteral(u"▶") : QStringLiteral(u"▼");
        p->drawText(QRect(left, top, 14, r.height()), Qt::AlignVCenter | Qt::AlignLeft, ch);
    }
    left += 16;

    // 章节时间范围
    QFont timeFont = p->font();
    timeFont.setFamily(QStringLiteral("Consolas"));
    timeFont.setPointSize(8);
    p->setFont(timeFont);
    p->setPen(kTimeGray);
    QString timeText = row.displayTime;
    p->drawText(QRect(left, top, 96, r.height()), Qt::AlignVCenter | Qt::AlignLeft, timeText);
    left += 100;

    // 章节标题
    QFont titleFont = p->font();
    titleFont.setFamily(QStringLiteral("Microsoft YaHei"));
    titleFont.setPointSize(9);
    titleFont.setBold(true);
    p->setFont(titleFont);

    QColor titleColor;
    if (m_panel->currentPosMs() >= row.endMs) {
        titleColor = kHeaderFinished;
    } else if (isCurrent) {
        titleColor = kHeaderCurrent;
    } else {
        titleColor = kHeaderUnread;
    }
    p->setPen(titleColor);
    QRect titleRect(left, top, r.right() - left - 10, r.height());
    QString elided = p->fontMetrics().elidedText(row.displayText, Qt::ElideRight, titleRect.width());
    p->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, elided);

    p->restore();
}

void TranscriptItemDelegate::paintParagraph(QPainter* p, const QStyleOptionViewItem& o, const RowItem& row,
                                            int rowInList) const {
    QRect r = o.rect;
    p->save();

    // 1) 收集每句在 displayText 中的 [start, end) 区间
    struct SubRun {
        int subIdx;
        int startUtf16;   // 包含
        int endUtf16;     // 不包含
        qint64 startMs;
        qint64 endMs;
    };
    QVector<SubRun> runs;
    {
        const QList<SubtitleItem>& subs = m_panel->subtitles();
        int cursor = 0;
        for (int si : row.subtitleIndices) {
            if (si < 0 || si >= subs.size()) continue;
            const QString txt = QString::fromStdString(subs[si].text).simplified();
            if (txt.isEmpty()) continue;
            int s = cursor;
            int e = cursor + txt.size();
            runs.append({si, s, e,
                         qint64(std::llround(subs[si].start_sec * 1000.0)),
                         qint64(std::llround(subs[si].end_sec   * 1000.0))});
            cursor = e + 1; // +1 是分隔空格
        }
    }

    // 2) 字体
    QFont textFont;
    textFont.setFamily(QStringLiteral("Microsoft YaHei"));
    textFont.setPointSize(9);
    p->setFont(textFont);
    QFontMetrics fm(textFont);

    int textLeft   = r.left() + kParaPadLeft;
    int textRight  = r.right() - kParaPadRight;
    int textWidth  = textRight - textLeft;
    if (textWidth < 10) textWidth = 10;
    int textTop    = r.top() + kParaPadTop;
    int textBottom = r.bottom() - kParaPadBottom;

    // 3) 段落白底
    p->fillRect(r, Qt::white);

    // 4) QTextLayout 折行
    QTextLayout layout(row.displayText, textFont);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opt);
    layout.beginLayout();
    QList<QTextLine> lines;
    qreal y = textTop;
    while (true) {
        QTextLine tl = layout.createLine();
        if (!tl.isValid()) break;
        tl.setLineWidth(textWidth);
        tl.setPosition(QPointF(textLeft, y));
        y += fm.height() + kParaLineSpacing;
        lines.append(tl);
    }
    layout.endLayout();

    // 清掉旧的本行 hit rects
    if (m_panel) m_panel->clearHitRectsForRow(rowInList);

    int activeIdx = m_panel->activeSubtitleIdx();
    qint64 curPos = m_panel->currentPosMs();
    bool wordLevel = m_panel->wordLevelEnabled();

    // 5) 准备"行 × 句子"的 spans 序列，用于有序绘制背景 + 字符
    struct Span {
        const QTextLine* tl;
        int a;        // 行内起点（含）
        int b;        // 行内终点（不含）
        int runIdx;   // 在 runs 中的索引，便于反查 run 信息
    };
    QList<Span> spans;
    spans.reserve(runs.size() * 2);
    for (int ri = 0; ri < runs.size(); ++ri) {
        const SubRun& run = runs[ri];
        if (run.endUtf16 <= run.startUtf16) continue;
        for (int li = 0; li < lines.size(); ++li) {
            const QTextLine& tl = lines[li];
            int ls = tl.textStart();
            int le = ls + tl.textLength();
            if (le <= run.startUtf16) continue;
            if (ls >= run.endUtf16) break;
            int a = qMax(ls, run.startUtf16);
            int b = qMin(le, run.endUtf16);
            if (b <= a) continue;
            spans.append({&tl, a, b, ri});
        }
    }

    // 行级矩形工具
    auto lineRectFor = [&](const QTextLine& tl, int a, int b) -> QRect {
        if (b <= a) return QRect();
        qreal xa = tl.cursorToX(a);
        qreal xb = tl.cursorToX(b);
        return QRect(QPointF(xa, tl.y()).toPoint(),
                     QPointF(xb, tl.y() + fm.height()).toPoint());
    };

    // 6) 画背景：每行每句分别 fill，不再跨行合并 → 不会盖下一句
    auto bgKindFor = [&](int subIdx) -> int {
        if (subIdx == m_panel->hoverSubtitleIdx()) return 2;
        if (subIdx == activeIdx) return 1;
        return 0;
    };
    for (const Span& sp : spans) {
        int kind = bgKindFor(runs[sp.runIdx].subIdx);
        if (kind == 0) continue;
        QRect bgRect = lineRectFor(*sp.tl, sp.a, sp.b);
        if (bgRect.isEmpty()) continue;
        QColor c;
        switch (kind) {
            case 1: c = kActiveBg;  break;
            case 2: c = kHoverBg;   break;
            default: continue;
        }
        p->fillRect(bgRect, c);
        if (kind == 1) {
            p->fillRect(bgRect.left(), bgRect.top(), 3, bgRect.height(), kAccentBlue);
        }
    }

    // 7) 记录 hit rect：每 subIdx 把所有行级矩形合并成一个，再扩边
    {
        QHash<int, QVector<QRect>> perSub;
        for (const Span& sp : spans) {
            int subIdx = runs[sp.runIdx].subIdx;
            QRect rr = lineRectFor(*sp.tl, sp.a, sp.b);
            if (!rr.isEmpty()) perSub[subIdx].append(rr);
        }
        for (auto it = perSub.constBegin(); it != perSub.constEnd(); ++it) {
            QRect hit;
            bool first = true;
            for (const QRect& rr : it.value()) {
                if (first) { hit = rr; first = false; }
                else        { hit = hit.united(rr); }
            }
            if (!hit.isEmpty()) {
                // hit rect 紧贴文字本身（不再外扩 3px），
                // 否则段落行间 padding 会被相邻句子的 hit rect 吞掉，
                // 鼠标停在"看起来是空白"的 padding 区会被识别成在某个句子上、显示 tooltip
                m_panel->addHitRect(rowInList, it.key(), hit);
            }
        }
    }

    // 8) 搜索关键词：在每个 run 范围内找 keyword 子串，
    //    画一个"更深"的背景条 + 重画字符作为"关键词高亮"（而不是整句染黄）
    struct KwHit { int a; int b; int runIdx; };
    QList<KwHit> kwHits;
    const QString kw = m_panel->searchKeyword();
    if (!kw.isEmpty()) {
        for (int ri = 0; ri < runs.size(); ++ri) {
            const SubRun& run = runs[ri];
            // 用 runs 里逐句找，避免跨句的误匹配；句内逐行扫
            QString text = row.displayText.mid(run.startUtf16, run.endUtf16 - run.startUtf16);
            int fromInRun = 0;
            while (fromInRun < text.size()) {
                int p = text.indexOf(kw, fromInRun, Qt::CaseInsensitive);
                if (p < 0) break;
                int a = run.startUtf16 + p;
                int b = a + kw.size();
                kwHits.append({a, b, ri});
                fromInRun = p + kw.size();
                if (kw.size() == 0) break;  // 防御空串死循环
            }
        }
        // 关键词背景：深黄底（与"整句浅黄"区分开）
        const QColor kKwBg(0xFF, 0xC1, 0x07);  // 亮金黄
        const QColor kKwFg(0x00, 0x00, 0x00);  // 黑字
        // 对每个 kwHit，按行切分（同一个 kwHit 可能跨多行，但同一句内基本不会），
        // 简化：直接 lineRectFor 拿矩形
        for (const KwHit& kh : kwHits) {
            for (const QTextLine& tl : lines) {
                int ls = tl.textStart();
                int le = ls + tl.textLength();
                if (le <= kh.a) continue;
                if (ls >= kh.b) break;
                int a = qMax(ls, kh.a);
                int b = qMin(le, kh.b);
                if (b <= a) continue;
                QRect r2 = lineRectFor(tl, a, b);
                if (r2.isEmpty()) continue;
                p->fillRect(r2, kKwBg);
                // 重画字符（黑字）
                p->setPen(kKwFg);
                p->drawText(QRectF(r2.left(), tl.y(), r2.width(), fm.height()),
                            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                            row.displayText.mid(a, b - a));
            }
        }
    }

    // 9) 画字符：按 spans 顺序逐字符绘制，rel 用"句首偏移"算 K 歌字级
    //    命中搜索关键词的位置跳过（已在 step 8 用更显眼的样式画过）
    auto isInKwHit = [&](int utf16Pos) -> bool {
        for (const KwHit& kh : kwHits) {
            if (utf16Pos >= kh.a && utf16Pos < kh.b) return true;
        }
        return false;
    };
    const QHash<int, SubtitleLineCache*> caches = m_panel->lineCache();
    for (const Span& sp : spans) {
        const QTextLine& tl = *sp.tl;
        const SubRun& run = runs[sp.runIdx];
        bool karaokeActive = (run.subIdx == activeIdx) && wordLevel;

        for (int i = sp.a; i < sp.b; ++i) {
            QChar c = row.displayText.at(i);
            if (c.isSpace()) continue;
            if (isInKwHit(i)) continue;  // 关键词子串已用深黄底+黑字画过
            QColor col;
            if (karaokeActive) {
                auto itc = caches.constFind(run.subIdx);
                SubtitleLineCache* cache = (itc != caches.constEnd()) ? itc.value() : nullptr;
                if (cache && !cache->words.isEmpty()) {
                    int rel = i - run.startUtf16;  // 字符在句内的 utf16 偏移
                    int wIdx = -1;
                    for (int wi = 0; wi < cache->words.size(); ++wi) {
                        const WordSegment& w = cache->words[wi];
                        if (rel >= w.startUtf16 && rel < w.endUtf16) { wIdx = wi; break; }
                    }
                    double posSec = curPos / 1000.0;
                    if (wIdx < 0) {
                        col = (curPos < run.startMs) ? kSubUnread
                             : (curPos >= run.endMs) ? kSubRead
                             : kSubRead;
                    } else {
                        const WordSegment& w = cache->words[wIdx];
                        if (posSec < w.startSec)      col = kSubUnread;
                        else if (posSec < w.endSec)   col = kKaraokeBlue;
                        else                          col = kSubRead;
                    }
                } else {
                    col = (curPos < run.startMs) ? kSubUnread
                         : (curPos >= run.endMs) ? kSubRead
                         : kSubRead;
                }
            } else {
                if (curPos < run.startMs)      col = kSubUnread;
                else if (curPos >= run.endMs)  col = kSubRead;
                else                            col = kSubRead;
            }
            qreal cx = tl.cursorToX(i);
            qreal cw = fm.horizontalAdvance(c);
            p->setPen(col);
            p->drawText(QRectF(cx, tl.y(), cw, fm.height()),
                        Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                        QString(c));
        }
    }

    p->restore();
}


// =============================================================
// TranscriptPanel
// =============================================================
TranscriptPanel::TranscriptPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("TranscriptPanel"));
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#FFFFFF"));
    setPalette(pal);

    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // ===== 工具栏 =====
    QWidget* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("TranscriptToolbar"));
    toolbar->setAutoFillBackground(true);
    QPalette tbPal = toolbar->palette();
    tbPal.setColor(QPalette::Window, QColor("#F9FAFB"));
    toolbar->setPalette(tbPal);
    QHBoxLayout* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(8, 4, 8, 4);
    tbLayout->setSpacing(4);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral(u"搜索关键词..."));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedHeight(24);

    m_chkWordLevel = new QCheckBox(QStringLiteral(u"逐字"), this);
    m_chkWordLevel->setChecked(true);
    m_chkWordLevel->setToolTip(QStringLiteral(u"当前句内字级 K 歌式变蓝"));
    m_chkWordLevel->setFixedHeight(24);

    m_btnCollapseFinished = new QPushButton(this);
    m_btnCollapseFinished->setText(QStringLiteral(u"折叠已播"));
    m_btnCollapseFinished->setToolTip(QStringLiteral(u"一键折叠所有已播完的章节"));
    m_btnCollapseFinished->setFixedHeight(24);
    m_btnCollapseFinished->setCursor(Qt::PointingHandCursor);

    m_btnExpandAll = new QPushButton(this);
    m_btnExpandAll->setText(QStringLiteral(u"全部展开"));
    m_btnExpandAll->setToolTip(QStringLiteral(u"展开所有章节"));
    m_btnExpandAll->setFixedHeight(24);
    m_btnExpandAll->setCursor(Qt::PointingHandCursor);

    m_btnCollapseAll = new QPushButton(this);
    m_btnCollapseAll->setText(QStringLiteral(u"全部折叠"));
    m_btnCollapseAll->setToolTip(QStringLiteral(u"折叠所有章节"));
    m_btnCollapseAll->setFixedHeight(24);
    m_btnCollapseAll->setCursor(Qt::PointingHandCursor);

    tbLayout->addWidget(m_search, 1);
    tbLayout->addWidget(m_chkWordLevel);
    tbLayout->addWidget(m_btnCollapseFinished);
    tbLayout->addWidget(m_btnExpandAll);
    tbLayout->addWidget(m_btnCollapseAll);
    vbox->addWidget(toolbar);

    // ===== 列表 =====
    m_list = new QListWidget(this);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setItemDelegate(new TranscriptItemDelegate(this, m_list));
    m_list->setUniformItemSizes(false);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setMouseTracking(true);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->viewport()->installEventFilter(this);
    m_list->viewport()->setMouseTracking(true);
    // 默认 list 占 stretch=1，empty 时改为 Ignored（避免不可见时仍撑高布局）
    m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vbox->addWidget(m_list, 1);

    // viewport 第一次有真实宽度时，重算所有 sizeHint
    // （首次 rebuildListWidget 时 viewport 可能尚未 layout，sizeHint 会拿到
    //  listWidth()=0 的兜底值，导致段落被错算成几十行高）
    QTimer::singleShot(0, this, [this]() {
        if (m_list && m_list->viewport()->width() > 0) {
            m_list->doItemsLayout();
        }
    });

    // ===== Empty 占位 =====
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setContentsMargins(16, 8, 16, 16);
    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #9CA3AF; font-size: 12px; font-family: Microsoft YaHei, sans-serif; background: #FFFFFF; line-height: 1.7; }"));
    m_emptyLabel->setText(QStringLiteral(u"📝 暂无字幕\n请先在「AI 视频总结」中点击「开始分析」"));
    m_emptyLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_emptyLabel->setVisible(true);
    vbox->addWidget(m_emptyLabel);

    // 启动就是 empty 状态：list 不可见 + Ignored，避免 list 仍按默认 sizeHint 撑高布局
    m_list->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_list->setVisible(false);

    // ===== 定时器 =====
    m_throttleTimer = new QTimer(this);
    m_throttleTimer->setSingleShot(true);
    m_throttleTimer->setInterval(16);
    connect(m_throttleTimer, &QTimer::timeout, this, &TranscriptPanel::onThrottleTimeout);

    m_autoScrollResumeTimer = new QTimer(this);
    m_autoScrollResumeTimer->setSingleShot(true);
    m_autoScrollResumeTimer->setInterval(3000);
    connect(m_autoScrollResumeTimer, &QTimer::timeout, this, &TranscriptPanel::onAutoScrollResume);

    // ===== 信号 =====
    connect(m_list, &QListWidget::itemDoubleClicked, this, &TranscriptPanel::onItemDoubleClicked);
    connect(m_search, &QLineEdit::textChanged, this, &TranscriptPanel::onSearchTextChanged);
    connect(m_btnCollapseFinished, &QPushButton::clicked, this, &TranscriptPanel::onCollapseFinishedClicked);
    connect(m_btnExpandAll, &QPushButton::clicked, this, &TranscriptPanel::onExpandAllClicked);
    connect(m_btnCollapseAll, &QPushButton::clicked, this, &TranscriptPanel::onCollapseAllClicked);
    connect(m_chkWordLevel, &QCheckBox::toggled, this, &TranscriptPanel::onWordLevelToggled);
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this, &TranscriptPanel::onScrollChanged);

    // ===== 样式 =====
    setStyleSheet(QString::fromLatin1(R"(
        TranscriptPanel { background-color: #FFFFFF; }
        QWidget#TranscriptToolbar {
            background-color: #F9FAFB;
            border-bottom: 1px solid #E5E7EB;
            margin: 0; padding: 0;
        }
        QLineEdit {
            border: 1px solid #E5E7EB;
            border-radius: 6px;
            padding: 3px 8px;
            background: #F9FAFB;
            font-size: 11px;
            font-family: Microsoft YaHei, sans-serif;
        }
        QLineEdit:focus { border-color: #0EA5E9; background: #FFFFFF; }
        QPushButton {
            border: 1px solid #E5E7EB;
            border-radius: 6px;
            padding: 0 8px;
            background: #FFFFFF;
            color: #374151;
            font-size: 11px;
            font-family: Microsoft YaHei, sans-serif;
        }
        QPushButton:hover { background: #F0F9FF; border-color: #0EA5E9; color: #0C4A6E; }
        QPushButton:pressed { background: #E0F2FE; }
        QCheckBox {
            color: #374151;
            font-size: 11px;
            font-family: Microsoft YaHei, sans-serif;
            padding: 0 4px;
        }
        QListWidget {
            border: none;
            background: #FFFFFF;
            outline: 0;
        }
        QListWidget::item { border: none; }
        QListWidget::item:selected { background: transparent; }
    )"));
}

TranscriptPanel::~TranscriptPanel() {
    qDeleteAll(m_lineCache);
    m_lineCache.clear();
}

void TranscriptPanel::setVideoPath(const QString& path) {
    if (path == m_currentVideoPath) return;
    m_currentVideoPath = path;
    clearAll();
}

void TranscriptPanel::clearAll() {
    m_subtitles.clear();
    m_chapters.clear();
    m_segments.clear();
    m_rows.clear();
    m_hitRects.clear();
    qDeleteAll(m_lineCache);
    m_lineCache.clear();
    m_searchHitRows.clear();
    if (m_list) m_list->clear();
    if (m_search) m_search->clear();
    m_searchKeyword.clear();
    m_currentPosMs = -1;
    m_activeSubtitleIdx = -1;
    m_activeChapterIdx = -1;
    m_hoverSubtitleIdx = -1;
    m_totalDurationMs = 0;
    m_hasChapters = m_hasSubtitles = m_hasSegments = false;
    m_autoScroll = true;
    if (m_emptyLabel) {
        m_emptyLabel->setVisible(true);
        m_emptyLabel->setText(QStringLiteral(u"📝 暂无字幕\n请先在「AI 视频总结」中点击「开始分析」"));
        m_emptyLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }
    if (m_list) {
        m_list->setVisible(false);
        m_list->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }
}

void TranscriptPanel::setSubtitleItems(const QList<SubtitleItem>& items) {
    m_subtitles = items;
    m_hasSubtitles = !items.isEmpty();
    qDeleteAll(m_lineCache);
    m_lineCache.clear();
    scheduleRebuildIfReady();
}

void TranscriptPanel::setChapters(const QList<SummaryChapter>& chapters) {
    m_chapters = chapters;
    m_hasChapters = !chapters.isEmpty();
    scheduleRebuildIfReady();
}

void TranscriptPanel::setSegments(const QList<SummarySegment>& segments) {
    m_segments = segments;
    m_hasSegments = !segments.isEmpty();
    scheduleRebuildIfReady();
}

void TranscriptPanel::setDuration(qint64 ms) {
    m_totalDurationMs = ms * 1000;
}

QString TranscriptPanel::formatTime(qint64 ms) const {
    if (ms < 0) ms = 0;
    int totalSec = int(ms / 1000);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    if (h > 0) return QString::asprintf("%d:%02d:%02d", h, m, s);
    return QString::asprintf("%02d:%02d", m, s);
}

void TranscriptPanel::scheduleRebuildIfReady() {
    if (!m_hasSubtitles) {
        // empty 状态：把 list 完全从布局中"忽略"掉（setVisible(false) 仍会占 stretch 空间），
        // 避免它在 vbox 里继续撑高，导致工具栏被往下推
        if (m_list) {
            m_list->setVisible(false);
            m_list->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        }
        if (m_emptyLabel) {
            m_emptyLabel->setVisible(true);
            m_emptyLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        }
        return;
    }
    rebuildRows();
    rebuildListWidget();
    if (m_emptyLabel) {
        m_emptyLabel->setVisible(false);
        m_emptyLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    }
    if (m_list) {
        m_list->setVisible(true);
        m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    if (!m_searchKeyword.isEmpty()) {
        onSearchTextChanged(m_searchKeyword);
    }
    if (m_currentPosMs >= 0) {
        applyHighlight(m_currentPosMs);
    }
}

void TranscriptPanel::rebuildRows() {
    m_rows.clear();

    struct Block { qint64 startMs, endMs; QString title; };
    QList<Block> blocks;

    if (!m_chapters.isEmpty()) {
        for (int i = 0; i < m_chapters.size(); ++i) {
            const auto& ch = m_chapters[i];
            QString title = ch.title.isEmpty()
                ? QStringLiteral(u"第 %1 段").arg(i + 1)
                : ch.title;
            blocks.append({ch.startMs, ch.endMs, title});
        }
    } else if (!m_segments.isEmpty()) {
        for (int i = 0; i < m_segments.size(); ++i) {
            const auto& seg = m_segments[i];
            blocks.append({seg.startMs, seg.endMs,
                           QStringLiteral(u"第 %1 段").arg(i + 1)});
        }
    } else if (!m_subtitles.isEmpty()) {
        blocks.append({0, m_totalDurationMs, QStringLiteral(u"全文")});
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
        header.collapsed = false;
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

void TranscriptPanel::rebuildListWidget() {
    if (!m_list) return;
    m_list->blockSignals(true);
    m_list->clear();
    for (int i = 0; i < m_rows.size(); ++i) {
        QListWidgetItem* it = new QListWidgetItem(m_list);
        it->setData(Qt::UserRole, i);
        it->setFlags(Qt::ItemIsEnabled);
    }
    applyCollapseToList();
    m_list->blockSignals(false);
}

void TranscriptPanel::applyCollapseToList() {
    if (!m_list) return;
    for (int i = 0; i < m_rows.size(); ++i) {
        const RowItem& r = m_rows[i];
        QListWidgetItem* it = m_list->item(i);
        if (!it) continue;
        if (r.type == RowType::Paragraph) {
            bool visible = true;
            for (const RowItem& h : m_rows) {
                if (h.type == RowType::ChapterHeader && h.chapterIndex == r.chapterIndex) {
                    visible = !h.collapsed;
                    break;
                }
            }
            it->setHidden(!visible);
        } else {
            it->setHidden(false);
        }
    }
}

// =============================================================
// Hit rects（paint 时更新，hover/click 用）
// =============================================================
void TranscriptPanel::clearHitRects() { m_hitRects.clear(); }
void TranscriptPanel::clearHitRectsForRow(int rowInList) {
    m_hitRects.erase(std::remove_if(m_hitRects.begin(), m_hitRects.end(),
        [rowInList](const HitRect& h){ return h.rowInList == rowInList; }),
        m_hitRects.end());
}
void TranscriptPanel::addHitRect(int rowInList, int subtitleIndex, const QRect& rect) {
    m_hitRects.append({rowInList, subtitleIndex, rect});
}
int TranscriptPanel::subtitleAtViewportPos(const QPoint& vp) const {
    for (int i = m_hitRects.size() - 1; i >= 0; --i) {
        if (m_hitRects[i].rect.contains(vp)) return m_hitRects[i].subtitleIndex;
    }
    return -1;
}
int TranscriptPanel::listWidth() const {
    if (!m_list) return 0;
    return m_list->viewport()->width();
}

QString TranscriptPanel::tooltipForSubtitle(int subtitleIndex) const {
    if (subtitleIndex < 0 || subtitleIndex >= m_subtitles.size()) return QString();
    const SubtitleItem& sub = m_subtitles[subtitleIndex];
    qint64 startMs = qint64(std::llround(sub.start_sec * 1000.0));
    qint64 endMs   = qint64(std::llround(sub.end_sec   * 1000.0));
    return QStringLiteral(u"⏱ %1 — %2")
        .arg(formatTime(startMs), formatTime(endMs));
}

// =============================================================
// 染色
// =============================================================
void TranscriptPanel::onPositionChanged(qint64 positionMs) {
    m_currentPosMs = positionMs * 1000;
    if (m_rows.isEmpty()) return;

    int newSubIdx = findActiveSubtitleIndex(m_currentPosMs);

    // 当前行（章节）没变 + 逐字开启 → 重绘当前 paragraph（K 歌持续变色）
    if (newSubIdx == m_activeSubtitleIdx && newSubIdx >= 0 && m_wordLevelEnabled) {
        int rowIdx = findRowIndexBySubtitle(newSubIdx);
        if (rowIdx >= 0) {
            QListWidgetItem* it = m_list->item(rowIdx);
            if (it) m_list->update(m_list->indexFromItem(it));
        }
        return;
    }

    if (!m_throttleTimer->isActive()) {
        m_throttleTimer->start();
    }
}

void TranscriptPanel::onThrottleTimeout() {
    applyHighlight(m_currentPosMs);
}

void TranscriptPanel::applyHighlight(qint64 posMs) {
    if (m_rows.isEmpty()) return;

    int newSubIdx = findActiveSubtitleIndex(posMs);
    int oldSubIdx = m_activeSubtitleIdx;
    m_activeSubtitleIdx = newSubIdx;

    int newChIdx = -1;
    if (newSubIdx >= 0) {
        int rowIdx = findRowIndexBySubtitle(newSubIdx);
        if (rowIdx >= 0) newChIdx = m_rows[rowIdx].chapterIndex;
    }
    int oldChIdx = m_activeChapterIdx;
    m_activeChapterIdx = newChIdx;

    bool rowChanged = (oldSubIdx != newSubIdx);
    bool chChanged  = (oldChIdx != newChIdx);

    if (!rowChanged && !chChanged) {
        if (newSubIdx >= 0 && m_wordLevelEnabled) {
            int r = findRowIndexBySubtitle(newSubIdx);
            if (r >= 0) {
                QListWidgetItem* it = m_list->item(r);
                if (it) m_list->update(m_list->indexFromItem(it));
            }
        }
        return;
    }

    QList<int> dirtyRows;
    if (oldSubIdx >= 0) {
        int r = findRowIndexBySubtitle(oldSubIdx);
        if (r >= 0) dirtyRows << r;
    }
    if (newSubIdx >= 0) {
        int r = findRowIndexBySubtitle(newSubIdx);
        if (r >= 0) dirtyRows << r;
    }
    if (chChanged) {
        if (oldChIdx >= 0) {
            int r = findRowIndexByChapter(oldChIdx);
            if (r >= 0) dirtyRows << r;
        }
        if (newChIdx >= 0) {
            int r = findRowIndexByChapter(newChIdx);
            if (r >= 0) dirtyRows << r;
        }
    }
    for (int r : dirtyRows) {
        QListWidgetItem* it = m_list->item(r);
        if (it) m_list->update(m_list->indexFromItem(it));
    }

    if (m_autoScroll) {
        scrollToActiveRow();
    }
}

int TranscriptPanel::findActiveSubtitleIndex(qint64 posMs) const {
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

int TranscriptPanel::findRowIndexBySubtitle(int subtitleIndex) const {
    if (subtitleIndex < 0) return -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        const RowItem& r = m_rows[i];
        // 段落：subtitleIndex 落入任一子句索引
        if (r.type == RowType::Paragraph && r.subtitleIndices.contains(subtitleIndex)) return i;
        if (r.type == RowType::ChapterHeader && r.subtitleIndex == subtitleIndex) return i;
    }
    return -1;
}

int TranscriptPanel::findRowIndexByChapter(int chapterIndex) const {
    if (chapterIndex < 0) return -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].type == RowType::ChapterHeader && m_rows[i].chapterIndex == chapterIndex) {
            return i;
        }
    }
    return -1;
}

int TranscriptPanel::getBlockState(int chapterIndex) const {
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

// =============================================================
// 交互
// 单击章节头：折叠/展开（不做任何 seek）
// 双击章节头：seek 到章节开始
// 单击/双击段落：seek 到该句开始
// =============================================================

// 在指定 row 上执行 seek（章节头→章节开始；段落→鼠标位置对应句子）
void TranscriptPanel::seekFromRow(int row, const QPoint& viewportPos) {
    if (row < 0 || row >= m_rows.size()) return;
    const RowItem& r = m_rows[row];
    if (r.type == RowType::ChapterHeader) {
        qint64 target = r.startMs;
        if (target < 0) target = 0;
        emit seekTo(target);
        return;
    }
    // 段落：用鼠标坐标定位到子句
    int subIdx = subtitleAtViewportPos(viewportPos);
    if (subIdx < 0) {
        subIdx = r.firstSubtitleIndex;
    }
    if (subIdx >= 0 && subIdx < m_subtitles.size()) {
        qint64 target = qint64(std::llround(m_subtitles[subIdx].start_sec * 1000.0));
        emit seekTo(target);
    }
}

// 暴露给代理：editorEvent 中收到双击时调用
void TranscriptPanel::seekFromRowAt(int row, const QPoint& viewportPos) {
    QPoint vp = viewportPos;
    if (vp.isNull() && m_list) {
        vp = m_list->viewport()->mapFromGlobal(QCursor::pos());
    }
    seekFromRow(row, vp);
}

void TranscriptPanel::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item || !m_list) return;
    int row = m_list->row(item);
    QPoint vp = m_list->viewport()->mapFromGlobal(QCursor::pos());
    seekFromRow(row, vp);
}

bool TranscriptPanel::handleHeaderClick(int rowInList, const QPoint& localPos, const QRect& itemRect) {
    if (rowInList < 0 || rowInList >= m_rows.size()) return false;
    const RowItem& r = m_rows[rowInList];
    if (r.type != RowType::ChapterHeader) {
        return false;
    }
    Q_UNUSED(localPos);
    Q_UNUSED(itemRect);
    // 章节头单击：只切换折叠 / 展开，不 seek（seek 改为双击）
    m_rows[rowInList].collapsed = !m_rows[rowInList].collapsed;
    applyCollapseToList();
    return true;
}

void TranscriptPanel::onCollapseFinishedClicked() {
    bool any = false;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].type != RowType::ChapterHeader) continue;
        if (getBlockState(m_rows[i].chapterIndex) == 2) {
            m_rows[i].collapsed = true;
            any = true;
        }
    }
    if (any) applyCollapseToList();
}

void TranscriptPanel::onExpandAllClicked() {
    bool any = false;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].type == RowType::ChapterHeader && m_rows[i].collapsed) {
            m_rows[i].collapsed = false;
            any = true;
        }
    }
    if (any) applyCollapseToList();
}

void TranscriptPanel::onCollapseAllClicked() {
    bool any = false;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].type == RowType::ChapterHeader && !m_rows[i].collapsed) {
            m_rows[i].collapsed = true;
            any = true;
        }
    }
    if (any) applyCollapseToList();
}

void TranscriptPanel::onSearchTextChanged(const QString& kw) {
    m_searchKeyword = kw;
    m_searchHitRows.clear();
    if (kw.isEmpty()) {
        if (m_list) m_list->viewport()->update();
        return;
    }
    // 命中改为"按 subIdx 记录"——段落里只要某个 sub 命中，整段都要参与高亮
    QSet<int> hitSubIdxs;
    for (int si = 0; si < m_subtitles.size(); ++si) {
        if (QString::fromStdString(m_subtitles[si].text).contains(kw, Qt::CaseInsensitive)) {
            hitSubIdxs.insert(si);
        }
    }
    // 找到第一个含命中的 row（用于滚动）
    int firstRow = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        const RowItem& r = m_rows[i];
        if (r.type != RowType::Paragraph) continue;
        bool hit = false;
        for (int si : r.subtitleIndices) if (hitSubIdxs.contains(si)) { hit = true; break; }
        if (hit) {
            m_searchHitRows.insert(r.firstSubtitleIndex);
            if (firstRow < 0) firstRow = i;
        }
    }
    if (m_list) {
        m_list->viewport()->update();
        if (firstRow >= 0) {
            m_list->scrollToItem(m_list->item(firstRow), QAbstractItemView::PositionAtCenter);
        }
    }
}

void TranscriptPanel::onWordLevelToggled(bool on) {
    m_wordLevelEnabled = on;
    if (m_list) m_list->viewport()->update();
}

void TranscriptPanel::onScrollChanged() {
    if (!m_autoScroll) return;
    m_autoScroll = false;
    m_autoScrollResumeTimer->start();
}

void TranscriptPanel::onAutoScrollResume() {
    m_autoScroll = true;
    if (m_activeSubtitleIdx >= 0) {
        scrollToActiveRow();
    }
}

void TranscriptPanel::scrollToActiveRow() {
    if (!m_list || m_activeSubtitleIdx < 0) return;
    int rowIdx = findRowIndexBySubtitle(m_activeSubtitleIdx);
    if (rowIdx < 0) return;
    QListWidgetItem* it = m_list->item(rowIdx);
    if (!it || it->isHidden()) return;
    m_list->scrollToItem(it, QAbstractItemView::PositionAtCenter);
}

// =============================================================
// 暴露给 ItemDelegate
// =============================================================
bool TranscriptPanel::isSearchHit(int subtitleIndex) const {
    return m_searchHitRows.contains(subtitleIndex);
}

int TranscriptPanel::activeWordInLine(int subtitleIndex, qint64 posMs) const {
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

// =============================================================
// 智能分词 + 字级线性插值（保留：K 歌需要）
// =============================================================
void TranscriptPanel::ensureLineCache(int subtitleIndex) {
    if (subtitleIndex < 0 || subtitleIndex >= m_subtitles.size()) return;
    if (m_lineCache.contains(subtitleIndex)) return;
    SubtitleLineCache* cache = new SubtitleLineCache(buildLineCache(subtitleIndex));
    m_lineCache.insert(subtitleIndex, cache);
}

SubtitleLineCache TranscriptPanel::buildLineCache(int subtitleIndex) const {
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

bool TranscriptPanel::isPunctuation(QChar c) {
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

QList<QPair<int,int>> TranscriptPanel::tokenize(const QString& text) {
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

double TranscriptPanel::computeTokenWeight(const QString& token) {
    if (token.isEmpty()) return 0.0;
    if (isPunctuation(token.at(0))) return 0.1;
    ushort u = token.at(0).unicode();
    if (token.size() == 1 && u >= 0x4E00 && u <= 0x9FFF) return 1.0;
    if (token.at(0).isLetterOrNumber()) return double(token.size()) * 1.2;
    return 1.0;
}

// =============================================================
// viewport eventFilter：mouse move / tooltip
// =============================================================
bool TranscriptPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_list->viewport()) {
        static int s_lastTooltipSub = -2;
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            // 严格过滤：只有鼠标真正在某个 item 上时，才看 hitRect
            QListWidgetItem* atItem = m_list->itemAt(me->pos());
            int newHover = atItem ? subtitleAtViewportPos(me->pos()) : -1;
            if (newHover != m_hoverSubtitleIdx) {
                int oldHover = m_hoverSubtitleIdx;
                m_hoverSubtitleIdx = newHover;
                auto rowOf = [&](int subIdx) -> int {
                    for (const HitRect& h : m_hitRects) {
                        if (h.subtitleIndex == subIdx) return h.rowInList;
                    }
                    return -1;
                };
                int rOld = (oldHover >= 0) ? rowOf(oldHover) : -1;
                int rNew = (newHover >= 0) ? rowOf(newHover) : -1;
                if (rOld >= 0 && m_list) m_list->update(m_list->indexFromItem(m_list->item(rOld)));
                if (rNew >= 0 && rNew != rOld && m_list) m_list->update(m_list->indexFromItem(m_list->item(rNew)));
            }
            if (newHover != s_lastTooltipSub) {
                s_lastTooltipSub = newHover;
                if (newHover >= 0) {
                    QToolTip::showText(me->globalPosition().toPoint(), tooltipForSubtitle(newHover), m_list->viewport());
                } else {
                    QToolTip::hideText();
                }
            } else if (newHover < 0) {
                // 稳定在空白区：保证不残留旧 tooltip（Windows 上 hideText 是非阻塞的，
                // 反复 move 时显式 hide 一次能让 Qt 内部 tooltip widget 立即消失）
                QToolTip::hideText();
            }
            return false;
        }
        if (event->type() == QEvent::Leave) {
            if (m_hoverSubtitleIdx >= 0) {
                int oldHover = m_hoverSubtitleIdx;
                m_hoverSubtitleIdx = -1;
                for (const HitRect& h : m_hitRects) {
                    if (h.subtitleIndex == oldHover && m_list) {
                        m_list->update(m_list->indexFromItem(m_list->item(h.rowInList)));
                        break;
                    }
                }
            }
            s_lastTooltipSub = -2;
            QToolTip::hideText();
            return false;
        }
        if (event->type() == QEvent::Resize) {
            // viewport 第一次有真实宽度时（首次 show / 父布局生效），重算所有 sizeHint
            // 否则 sizeHint 在 listWidth()==0 时会拿兜底值，段落被错算为几十行高
            static int s_lastW = -1;
            int curW = m_list->viewport()->width();
            if (curW > 0 && curW != s_lastW) {
                s_lastW = curW;
                QTimer::singleShot(0, this, [this]() {
                    if (m_list) m_list->doItemsLayout();
                });
            }
            return false;
        }
    }
    return QWidget::eventFilter(watched, event);
}
