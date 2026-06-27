#include "slidingtabwidget.h"

#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>

namespace {
// 配色：浅灰方块作为滑块背景
const QColor kSliderFill    (229, 231, 235, 255);  // #E5E7EB 浅灰
const QColor kSliderText    (17,  24,  39,  255);  // #111827 选中 tab 文字：近黑
const QColor kUnselectedText(107, 114, 128, 255);  // #6B7280 未选中 tab 文字：灰
const int    kTabBarHeight  = 36;
const int    kSliderHeight  = 30;
const int    kSliderMarginX = 4;   // 滑块左右各留 4px 间隙
const int    kSliderRadius  = 6;   // 方块圆角
const int    kSliderMarginY = (kTabBarHeight - kSliderHeight) / 2;
}

// ============================================================
// SlidingTabBar —— 真正负责绘制滑块和文字的子控件
// ============================================================
SlidingTabBar::SlidingTabBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("slidingTabBar");
    setFixedHeight(kTabBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 关键：不设 QSS 背景，让 paintEvent 完全掌控绘制
    setAttribute(Qt::WA_OpaquePaintEvent, true);   // 提高绘制效率
}

int SlidingTabBar::tabWidth() const {
    int n = m_labels.size();
    if (n <= 0) return 0;
    return width() / n;
}

void SlidingTabBar::setCurrentIndex(int idx) {
    if (m_currentIndex == idx) return;
    m_currentIndex = idx;
    update();
}

void SlidingTabBar::setSliderPositionPx(int px) {
    if (m_sliderPosition == px) return;
    m_sliderPosition = px;
    update();
}

void SlidingTabBar::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1) 底色（纯白）
    p.fillRect(rect(), QColor(255, 255, 255));

    int n = m_labels.size();
    if (n <= 0) return;
    int tabW = tabWidth();
    if (tabW <= 0) return;

    // 2) 画滑块（圆角方块背景，浅灰）
    QRectF sliderRect(m_sliderPosition, kSliderMarginY,
                      tabW - 2 * kSliderMarginX, kSliderHeight);
    p.setPen(Qt::NoPen);
    p.setBrush(kSliderFill);
    p.drawRoundedRect(sliderRect, kSliderRadius, kSliderRadius);

    // 3) 画每个 tab 的文字
    for (int i = 0; i < n; ++i) {
        QRect tabRect(i * tabW, 0, tabW, kTabBarHeight);
        bool selected = (i == m_currentIndex);

        QFont f = p.font();
        f.setPixelSize(13);
        f.setFamily(QStringLiteral("Microsoft YaHei, PingFang SC, sans-serif"));
        f.setBold(selected);
        p.setFont(f);
        p.setPen(selected ? kSliderText : kUnselectedText);
        p.drawText(tabRect, Qt::AlignCenter, m_labels.at(i));
    }
}

void SlidingTabBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    int n = m_labels.size();
    if (n <= 0) {
        QWidget::mousePressEvent(event);
        return;
    }
    int tabW = tabWidth();
    if (tabW <= 0) {
        QWidget::mousePressEvent(event);
        return;
    }
    int idx = event->pos().x() / tabW;
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    emit currentIndexChangedByClick(idx);
    event->accept();
}

// ============================================================
// SlidingTabWidget —— 容器：管理 tab 列表、stack、动画
// ============================================================
SlidingTabWidget::SlidingTabWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("SlidingTabWidget");

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 顶部 Tab 栏（自带绘制）
    m_tabBar = new SlidingTabBar(this);
    root->addWidget(m_tabBar);

    // 内容区
    m_stack = new QStackedWidget(this);
    m_stack->setStyleSheet(QStringLiteral("QStackedWidget { background-color: #FFFFFF; }"));
    root->addWidget(m_stack, 1);

    // 点击信号
    connect(m_tabBar, &SlidingTabBar::currentIndexChangedByClick,
            this, &SlidingTabWidget::onBarClicked);

    // 滑块动画（驱动 m_tabBar->setSliderPositionPx）
    m_anim = new QPropertyAnimation(this, "sliderPosition", this);
    m_anim->setDuration(m_animDuration);
    m_anim->setEasingCurve(QEasingCurve::InOutCubic);
}

SlidingTabWidget::~SlidingTabWidget() {
    m_tabBar = nullptr;
}

int SlidingTabWidget::addTab(QWidget* page, const QString& label) {
    int idx = m_labels.size();
    m_labels.append(label);
    m_stack->addWidget(page);

    // 把新 label 推到 SlidingTabBar
    m_tabBar->setLabels(m_labels);

    // 第一次 addTab 时初始化 currentIndex
    if (m_currentIndex < 0) {
        m_currentIndex = idx;
        m_stack->setCurrentIndex(idx);
        m_tabBar->setCurrentIndex(idx);
        QTimer::singleShot(0, this, [this]() { updateSliderGeometry(false); });
    }
    return idx;
}

int SlidingTabWidget::currentIndex() const {
    return m_currentIndex;
}

void SlidingTabWidget::setCurrentIndex(int index) {
    if (index < 0 || index >= m_labels.size()) return;
    if (index == m_currentIndex) {
        m_stack->setCurrentIndex(index);
        return;
    }
    m_currentIndex = index;
    m_stack->setCurrentIndex(index);
    m_tabBar->setCurrentIndex(index);
    updateSliderGeometry(true);
    emit currentChanged(index);
}

int SlidingTabWidget::sliderPosition() const {
    if (!m_tabBar) return 0;
    return m_tabBar->sliderPositionPx();
}

void SlidingTabWidget::setSliderPosition(int pos) {
    if (!m_tabBar) return;
    m_tabBar->setSliderPositionPx(pos);
    emit sliderPositionChanged(pos);
}

void SlidingTabWidget::updateSliderGeometry(bool animate) {
    if (m_currentIndex < 0) return;
    if (!m_tabBar) return;
    int tabW = m_tabBar->tabWidth();
    if (tabW <= 0) return;

    int target = m_currentIndex * tabW + kSliderMarginX;

    if (animate && sliderPosition() != target) {
        m_anim->stop();
        m_anim->setStartValue(sliderPosition());
        m_anim->setEndValue(target);
        m_anim->start();
    } else {
        m_anim->stop();
        setSliderPosition(target);
    }
}

void SlidingTabWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // 窗口/容器大小变化时按比例更新滑块位置（不带动画，避免抖动）
    updateSliderGeometry(false);
}

void SlidingTabWidget::onBarClicked(int index) {
    setCurrentIndex(index);
}