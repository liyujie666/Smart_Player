#ifndef SLIDINGTABWIDGET_H
#define SLIDINGTABWIDGET_H

#include <QWidget>
#include <QStackedWidget>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QList>
#include <QString>

class QLabel;
class QHBoxLayout;
class QPainter;
class QPaintEvent;
class QMouseEvent;
class QResizeEvent;

// 内部 TabBar 控件：自己负责绘制"胶囊背景滑块"和文字
// （必须独立绘制，否则会被父 widget / QSS 背景覆盖）
class SlidingTabBar : public QWidget {
    Q_OBJECT
public:
    explicit SlidingTabBar(QWidget* parent = nullptr);

    void setLabels(const QList<QString>& labels) { m_labels = labels; update(); }
    void setCurrentIndex(int idx);
    int  currentIndex() const { return m_currentIndex; }
    void setSliderPositionPx(int px);
    int  sliderPositionPx() const { return m_sliderPosition; }

    // 单个 tab 的宽度（像素） = tabBar.width() / tabCount
    int  tabWidth() const;

signals:
    void currentIndexChangedByClick(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QList<QString> m_labels;
    int  m_currentIndex   = -1;
    int  m_sliderPosition = 0;          // 像素 x（由 SlidingTabWidget 通过 QPropertyAnimation 驱动）
};

/**
 * @brief 带"方块滑块"切换动画的 Tab 控件
 *
 *  - 所有 tab 按钮等宽，平分整个 Tab 栏宽度
 *  - 切换时浅灰色"圆角方块"在按钮之间平滑滑动（默认 200ms）
 *  - 内容区用 QStackedWidget 承载
 */
class SlidingTabWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int sliderPosition READ sliderPosition WRITE setSliderPosition NOTIFY sliderPositionChanged)

public:
    explicit SlidingTabWidget(QWidget* parent = nullptr);
    ~SlidingTabWidget() override;

    int  addTab(QWidget* page, const QString& label);

    int  currentIndex() const;
    void setCurrentIndex(int index);

    int  sliderPosition() const;
    void setSliderPosition(int pos);

    void setAnimationDuration(int ms) { m_animDuration = ms; if (m_anim) m_anim->setDuration(ms); }
    int  animationDuration() const { return m_animDuration; }

signals:
    void currentChanged(int index);
    void sliderPositionChanged(int pos);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onBarClicked(int index);

private:
    void updateSliderGeometry(bool animate);

    // UI
    SlidingTabBar*   m_tabBar = nullptr;
    QHBoxLayout*     m_tabBarLayout = nullptr;   // 占位：保留以备未来使用
    QStackedWidget*  m_stack = nullptr;

    // 状态
    QList<QString>  m_labels;
    int  m_currentIndex = -1;
    int  m_animDuration = 200;

    // 滑块动画
    QPropertyAnimation* m_anim = nullptr;
};

#endif // SLIDINGTABWIDGET_H