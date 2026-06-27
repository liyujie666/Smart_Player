#include "videoslider.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QStyle>

VideoSlider::VideoSlider(QWidget *parent) : QSlider(parent) {
    setMouseTracking(true); // 实时监听鼠标
    timer_.start();
}

void VideoSlider::mouseReleaseEvent(QMouseEvent *ev) {
    int value = QStyle::sliderValueFromPosition(minimum(), maximum(), ev->pos().x(), width());
    setValue(value);
    // 先调基类实现，让它完成 setSliderDown(false) 和可能的 sliderReleased 流程。
    QSlider::mouseReleaseEvent(ev);
    // Qt 默认仅在拖动 thumb 时才 emit sliderPressed / sliderReleased，
    // 直接点击 groove 时不会 emit。MainWindow::on_progressSlider_sliderReleased
    // 才是真正发起 seek 的地方，所以这里补一次 sliderReleased，确保
    // "点击"和"拖动"行为一致。
    if (ev->button() == Qt::LeftButton) {
        emit sliderReleased();
    }
}


void VideoSlider::mouseMoveEvent(QMouseEvent *ev){
    if (timer_.elapsed() < 10) {
        QSlider::mouseMoveEvent(ev);
        return;
    }
    timer_.restart();
    int seekTime = QStyle::sliderValueFromPosition(minimum(), maximum(), ev->pos().x(), width());
    emit preview(seekTime, ev->pos().x());
    QSlider::mouseMoveEvent(ev);
}

void VideoSlider::leaveEvent(QEvent *ev){
    emit mouseleave();
}

void VideoSlider::keyPressEvent(QKeyEvent *ev)
{
    // QAbstractSlider 默认会把方向键翻译成 ±singleStep 的 setValue 并 accept，
    // 导致事件不再冒泡到 MainWindow，破坏键盘快进/快退的全局快捷键。
    // 这里把方向键 ignore 掉，让 MainWindow::keyPressEvent 统一处理。
    switch (ev->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
        ev->ignore();
        return;
    default:
        QSlider::keyPressEvent(ev);
    }
}

void VideoSlider::changeValue(int n){
    setValue(value() + n);
}
