#include "videoslider.h"
#include <QMouseEvent>
#include <QStyle>

VideoSlider::VideoSlider(QWidget *parent) : QSlider(parent) {
    setMouseTracking(true); // 实时监听鼠标
    timer_.start();
}

void VideoSlider::mouseReleaseEvent(QMouseEvent *ev) {
    int value = QStyle::sliderValueFromPosition(minimum(), maximum(), ev->pos().x(), width());
    setValue(value);
    QSlider::mouseReleaseEvent(ev);
    emit clicked(this);
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

void VideoSlider::changeValue(int n){
    setValue(value() + n);
    emit clicked(this);
}
