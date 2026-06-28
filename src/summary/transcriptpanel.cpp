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
// 内部工具（仅 UI 绘制相关）
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

static const int kParaPadLeft      = 8;
static const int kParaPadRight     = 10;
static const int kParaPadTop       = 6;
static const int kParaPadBottom    = 6;
static const int kParaLineSpacing  = 2;

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
    QFont textFont;
    textFont.setFamily(QStringLiteral("Microsoft YaHei"));
    textFont.setPointSize(9);
    QFontMetrics fm(textFont);

    int textWidth = w - kParaPadLeft - kParaPadRight;

    if (textWidth < 60) {
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
        return m_panel->handleHeaderClick(index.row(), local, option.rect);
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
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

    static const QPixmap s_arrowDown  = QPixmap(QStringLiteral(":/SmartPlayer-icon/arrow_down_dark.png"));
    static const QPixmap s_arrowRight = QPixmap(QStringLiteral(":/SmartPlayer-icon/arrow_right_dark.png"));
    const QPixmap& arrow = row.collapsed ? s_arrowRight : s_arrowDown;
    int iconSize = 12;
    int iconY = r.top() + (r.height() - iconSize) / 2;
    if (!arrow.isNull()) {
        p->drawPixmap(left, iconY, iconSize, iconSize, arrow);
    } else {
        p->setPen(kHeaderUnread);
        QFont iconFont = p->font();
        iconFont.setPointSize(7);
        p->setFont(iconFont);
        QString ch = row.collapsed ? QStringLiteral(u"▶") : QStringLiteral(u"▼");
        p->drawText(QRect(left, top, 14, r.height()), Qt::AlignVCenter | Qt::AlignLeft, ch);
    }
    left += 16;

    QFont timeFont = p->font();
    timeFont.setFamily(QStringLiteral("Consolas"));
    timeFont.setPointSize(8);
    p->setFont(timeFont);
    p->setPen(kTimeGray);
    QString timeText = row.displayTime;
    p->drawText(QRect(left, top, 96, r.height()), Qt::AlignVCenter | Qt::AlignLeft, timeText);
    left += 100;

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

    struct SubRun {
        int subIdx;
        int startUtf16;
        int endUtf16;
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
            cursor = e + 1;
        }
    }

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

    p->fillRect(r, Qt::white);

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

    if (m_panel) m_panel->clearHitRectsForRow(rowInList);

    int activeIdx = m_panel->activeSubtitleIdx();
    qint64 curPos = m_panel->currentPosMs();
    bool wordLevel = m_panel->wordLevelEnabled();

    struct Span {
        const QTextLine* tl;
        int a;
        int b;
        int runIdx;
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

    auto lineRectFor = [&](const QTextLine& tl, int a, int b) -> QRect {
        if (b <= a) return QRect();
        qreal xa = tl.cursorToX(a);
        qreal xb = tl.cursorToX(b);
        return QRect(QPointF(xa, tl.y()).toPoint(),
                     QPointF(xb, tl.y() + fm.height()).toPoint());
    };

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
                m_panel->addHitRect(rowInList, it.key(), hit);
            }
        }
    }

    struct KwHit { int a; int b; int runIdx; };
    QList<KwHit> kwHits;
    const QString kw = m_panel->searchKeyword();
    if (!kw.isEmpty()) {
        for (int ri = 0; ri < runs.size(); ++ri) {
            const SubRun& run = runs[ri];
            QString text = row.displayText.mid(run.startUtf16, run.endUtf16 - run.startUtf16);
            int fromInRun = 0;
            while (fromInRun < text.size()) {
                int pIdx = text.indexOf(kw, fromInRun, Qt::CaseInsensitive);
                if (pIdx < 0) break;
                int a = run.startUtf16 + pIdx;
                int b = a + kw.size();
                kwHits.append({a, b, ri});
                fromInRun = pIdx + kw.size();
                if (kw.size() == 0) break;
            }
        }
        const QColor kKwBg(0xFF, 0xC1, 0x07);
        const QColor kKwFg(0x00, 0x00, 0x00);
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
                p->setPen(kKwFg);
                p->drawText(QRectF(r2.left(), tl.y(), r2.width(), fm.height()),
                            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                            row.displayText.mid(a, b - a));
            }
        }
    }

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
            if (isInKwHit(i)) continue;
            QColor col;
            if (karaokeActive) {
                auto itc = caches.constFind(run.subIdx);
                SubtitleLineCache* cache = (itc != caches.constEnd()) ? itc.value() : nullptr;
                if (cache && !cache->words.isEmpty()) {
                    int rel = i - run.startUtf16;
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
    m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vbox->addWidget(m_list, 1);

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
            spacing: 4px;
            padding: 0;
        }
        QCheckBox::indicator {
            width: 13px;
            height: 13px;
            border: 1.5px solid #D1D5DB;
            border-radius: 3px;
            background-color: #FFFFFF;
        }
        QCheckBox::indicator:hover {
            border-color: #38BDF8;
            background-color: #F0F9FF;
        }
        QCheckBox::indicator:checked {
            border-color: #0EA5E9;
            background-color: #0EA5E9;
            image: url(:/SmartPlayer-icon/check_white.png);
        }
        QCheckBox::indicator:checked:hover {
            background-color: #38BDF8;
            border-color: #38BDF8;
        }
        QCheckBox::indicator:disabled {
            border-color: #E5E7EB;
            background-color: #F9FAFB;
        }
        QCheckBox:disabled { color: #9CA3AF; }
        QListWidget {
            border: none;
            background: #FFFFFF;
            outline: 0;
        }
        QListWidget::item { border: none; }
        QListWidget::item:selected { background: transparent; }
        /* 滚动条：与 fileList 同款暗色细条 */
        TranscriptPanel QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 4px 2px 4px 0;
            border: none;
        }
        TranscriptPanel QScrollBar::handle:vertical {
            background: #D1D5DB;
            min-height: 40px;
            border-radius: 4px;
            border: none;
        }
        TranscriptPanel QScrollBar::handle:vertical:hover { background: #9CA3AF; }
        TranscriptPanel QScrollBar::handle:vertical:pressed { background: #6B7280; }
        TranscriptPanel QScrollBar::add-line:vertical,
        TranscriptPanel QScrollBar::sub-line:vertical {
            background: none;
            border: none;
            height: 0px;
        }
        TranscriptPanel QScrollBar::add-page:vertical,
        TranscriptPanel QScrollBar::sub-page:vertical {
            background: none;
            border: none;
        }
    )"));
}

TranscriptPanel::~TranscriptPanel() = default;

// =============================================================
// VM 绑定
// =============================================================
void TranscriptPanel::bindViewModel(TranscriptViewModel* vm) {
    m_vm = vm;
    if (!m_vm) return;

    connect(m_vm, &TranscriptViewModel::rowsRebuilt,
            this, &TranscriptPanel::onVmRowsRebuilt);
    connect(m_vm, &TranscriptViewModel::rowsCollapseChanged,
            this, &TranscriptPanel::onVmRowsCollapseChanged);
    connect(m_vm, &TranscriptViewModel::activeIndexChanged,
            this, &TranscriptPanel::onVmActiveIndexChanged);
    connect(m_vm, &TranscriptViewModel::searchChanged,
            this, &TranscriptPanel::onVmSearchChanged);
    connect(m_vm, &TranscriptViewModel::dataReady,
            this, &TranscriptPanel::onVmDataReady);

    // 用户意图：VM 把 "请求 seek" 通过 panel.seekTo 信号广播给 MainWindow
    connect(m_vm, &TranscriptViewModel::seekRequested,
            this, &TranscriptPanel::seekTo);

    // 初始同步一次
    onVmDataReady();
    onVmRowsRebuilt();
}

// =============================================================
// 透传到 VM 的便捷 slot
// =============================================================
void TranscriptPanel::setVideoPath(const QString& path) {
    if (!m_vm) return;
    m_vm->setVideoPath(path);
}

void TranscriptPanel::clearAll() {
    if (!m_vm) return;
    m_vm->clearAll();
    m_hitRects.clear();
    m_hoverSubtitleIdx = -1;
    if (m_search) m_search->clear();
}

void TranscriptPanel::setSubtitleItems(const QList<SubtitleItem>& items) {
    if (!m_vm) return;
    m_vm->setSubtitles(items);
}

void TranscriptPanel::setChapters(const QList<SummaryChapter>& chapters) {
    if (!m_vm) return;
    m_vm->setChapters(chapters);
}

void TranscriptPanel::setSegments(const QList<SummarySegment>& segments) {
    if (!m_vm) return;
    m_vm->setSegments(segments);
}

void TranscriptPanel::setDuration(qint64 ms) {
    if (!m_vm) return;
    m_vm->setDuration(ms);
}

void TranscriptPanel::onPositionChanged(qint64 positionMs) {
    if (!m_vm) return;
    m_pendingPositionMs = positionMs;
    if (m_vm->rows().isEmpty()) return;

    // 当前句不变 + 逐字开启 → 立即重绘当前 paragraph（K 歌持续变色）
    int newSubIdx = m_vm->findActiveSubtitleIndex(positionMs * 1000);
    if (newSubIdx == m_vm->activeSubtitleIdx() && newSubIdx >= 0 && m_vm->wordLevelEnabled()) {
        // 仅同步 VM 内部 currentPosMs（不重算 active），由 onVmActiveIndex 不会触发，View 直接重绘
        m_vm->updatePosition(positionMs);
        int rowIdx = m_vm->findRowIndexBySubtitle(newSubIdx);
        if (rowIdx >= 0 && m_list) {
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
    if (!m_vm) return;
    m_vm->updatePosition(m_pendingPositionMs);
}

// =============================================================
// 数据访问透传
// =============================================================
const QList<RowItem>& TranscriptPanel::rows() const {
    static const QList<RowItem> kEmpty;
    return m_vm ? m_vm->rows() : kEmpty;
}
int TranscriptPanel::activeSubtitleIdx() const { return m_vm ? m_vm->activeSubtitleIdx() : -1; }
int TranscriptPanel::activeChapterIdx()  const { return m_vm ? m_vm->activeChapterIdx()  : -1; }
qint64 TranscriptPanel::currentPosMs()   const { return m_vm ? m_vm->currentPositionMs() : -1; }
bool TranscriptPanel::wordLevelEnabled() const { return m_vm ? m_vm->wordLevelEnabled() : true; }
bool TranscriptPanel::isSearchHit(int subtitleIndex) const { return m_vm && m_vm->isSearchHit(subtitleIndex); }
int  TranscriptPanel::activeWordInLine(int subtitleIndex, qint64 posMs) const {
    return m_vm ? m_vm->activeWordInLine(subtitleIndex, posMs) : -1;
}
const QList<SubtitleItem>& TranscriptPanel::subtitles() const {
    static const QList<SubtitleItem> kEmpty;
    return m_vm ? m_vm->subtitles() : kEmpty;
}
QHash<int, SubtitleLineCache*> TranscriptPanel::lineCache() const {
    return m_vm ? m_vm->lineCache() : QHash<int, SubtitleLineCache*>{};
}
QString TranscriptPanel::tooltipForSubtitle(int subtitleIndex) const {
    return m_vm ? m_vm->tooltipForSubtitle(subtitleIndex) : QString();
}
QString TranscriptPanel::searchKeyword() const {
    return m_vm ? m_vm->searchKeyword() : QString();
}

// =============================================================
// Hit rects（View 局部 UI 状态）
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

// =============================================================
// 渲染响应 VM 的 signal
// =============================================================
void TranscriptPanel::onVmRowsRebuilt() {
    rebuildListWidget();
    if (m_vm && !m_vm->searchKeyword().isEmpty()) {
        // 重建后强制刷新视口
        if (m_list) m_list->viewport()->update();
    }
}

void TranscriptPanel::onVmRowsCollapseChanged() {
    applyCollapseToList();
}

void TranscriptPanel::onVmActiveIndexChanged() {
    if (!m_list) return;
    // 只更新视口，让 ItemDelegate 重绘当前活动 row + 邻近 row
    m_list->viewport()->update();
    if (m_vm && m_vm->autoScroll()) {
        scrollToActiveRow();
    }
}

void TranscriptPanel::onVmSearchChanged(const QString& /*kw*/) {
    if (!m_list || !m_vm) return;
    m_list->viewport()->update();
    // 滚到第一个命中的段落 row
    int firstRow = -1;
    const QList<RowItem>& rs = m_vm->rows();
    for (int i = 0; i < rs.size(); ++i) {
        if (rs[i].type != RowType::Paragraph) continue;
        for (int si : rs[i].subtitleIndices) {
            if (m_vm->isSearchHit(si)) { firstRow = i; break; }
        }
        if (firstRow >= 0) break;
    }
    if (firstRow >= 0) {
        m_list->scrollToItem(m_list->item(firstRow), QAbstractItemView::PositionAtCenter);
    }
}

void TranscriptPanel::onVmDataReady() {
    if (!m_vm) return;
    bool empty = !m_vm->hasSubtitles();
    if (empty) {
        if (m_list) {
            m_list->setVisible(false);
            m_list->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        }
        if (m_emptyLabel) {
            m_emptyLabel->setVisible(true);
            m_emptyLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        }
    } else {
        if (m_emptyLabel) {
            m_emptyLabel->setVisible(false);
            m_emptyLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
        }
        if (m_list) {
            m_list->setVisible(true);
            m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }
    }
}

// =============================================================
// View → VM 命令转发
// =============================================================
void TranscriptPanel::onSearchTextChanged(const QString& kw) {
    if (!m_vm) return;
    m_vm->setSearchKeyword(kw);
}

void TranscriptPanel::onWordLevelToggled(bool on) {
    if (!m_vm) return;
    m_vm->setWordLevelEnabled(on);
    if (m_list) m_list->viewport()->update();
}

void TranscriptPanel::onCollapseFinishedClicked() { if (m_vm) m_vm->collapseAllFinishedChapters(); }
void TranscriptPanel::onExpandAllClicked()        { if (m_vm) m_vm->expandAll(); }
void TranscriptPanel::onCollapseAllClicked()      { if (m_vm) m_vm->collapseAll(); }

void TranscriptPanel::onScrollChanged() {
    if (!m_vm) return;
    if (!m_vm->autoScroll()) return;
    m_vm->setAutoScroll(false);
    m_autoScrollResumeTimer->start();
}

void TranscriptPanel::onAutoScrollResume() {
    if (!m_vm) return;
    m_vm->setAutoScroll(true);
    if (m_vm->activeSubtitleIdx() >= 0) {
        scrollToActiveRow();
    }
}

// =============================================================
// list rebuild / collapse
// =============================================================
void TranscriptPanel::rebuildListWidget() {
    if (!m_list) return;
    const QList<RowItem>& rs = rows();
    m_list->blockSignals(true);
    m_list->clear();
    for (int i = 0; i < rs.size(); ++i) {
        QListWidgetItem* it = new QListWidgetItem(m_list);
        it->setData(Qt::UserRole, i);
        it->setFlags(Qt::ItemIsEnabled);
    }
    applyCollapseToList();
    m_list->blockSignals(false);
}

void TranscriptPanel::applyCollapseToList() {
    if (!m_list) return;
    const QList<RowItem>& rs = rows();
    for (int i = 0; i < rs.size(); ++i) {
        const RowItem& r = rs[i];
        QListWidgetItem* it = m_list->item(i);
        if (!it) continue;
        if (r.type == RowType::Paragraph) {
            bool visible = true;
            for (const RowItem& h : rs) {
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
// seek 路径
// =============================================================
void TranscriptPanel::seekFromRow(int row, const QPoint& viewportPos) {
    if (!m_vm) return;
    const QList<RowItem>& rs = m_vm->rows();
    if (row < 0 || row >= rs.size()) return;
    const RowItem& r = rs[row];
    if (r.type == RowType::ChapterHeader) {
        m_vm->requestSeekToChapter(r.chapterIndex);
        return;
    }
    int subIdx = subtitleAtViewportPos(viewportPos);
    if (subIdx < 0) subIdx = r.firstSubtitleIndex;
    if (subIdx >= 0) m_vm->requestSeekToSubtitle(subIdx);
}

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
    Q_UNUSED(localPos);
    Q_UNUSED(itemRect);
    if (!m_vm) return false;
    const QList<RowItem>& rs = m_vm->rows();
    if (rowInList < 0 || rowInList >= rs.size()) return false;
    const RowItem& r = rs[rowInList];
    if (r.type != RowType::ChapterHeader) return false;
    m_vm->toggleChapterCollapsed(r.chapterIndex);
    return true;
}

void TranscriptPanel::scrollToActiveRow() {
    if (!m_list || !m_vm) return;
    int sub = m_vm->activeSubtitleIdx();
    if (sub < 0) return;
    int rowIdx = m_vm->findRowIndexBySubtitle(sub);
    if (rowIdx < 0) return;
    QListWidgetItem* it = m_list->item(rowIdx);
    if (!it || it->isHidden()) return;
    m_list->scrollToItem(it, QAbstractItemView::PositionAtCenter);
}

// =============================================================
// viewport eventFilter：mouse move / tooltip
// =============================================================
bool TranscriptPanel::eventFilter(QObject* watched, QEvent* event) {
    if (m_list && watched == m_list->viewport()) {
        static int s_lastTooltipSub = -2;
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
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
