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
