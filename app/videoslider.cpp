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
    QSlider::mouseReleaseEvent(ev);
    // 注意：这里不要 emit clicked(this)——
    // QSlider::mouseReleaseEvent 内部已经 emit 了 sliderReleased，
    // 而 mainwindow 在 sliderReleased 里就会调用 player_->seek(value)。
    // 若再 emit clicked，会再触发一次 onSliderClicked 里的 seek，
    // 而那一次 seek 内部 emit timeChanged，会在 sync_clock 被 reset 后
    // 把 progressSlider->value 砸回 0，导致第二次 seek 跳到开头。
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
