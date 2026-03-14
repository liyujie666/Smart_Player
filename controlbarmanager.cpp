#include "controlbarmanager.h"


ControlBarManager::ControlBarManager(VideoWidget* videoWidget,QWidget* originalParent, QWidget* controlBar, QObject* parent)
    : QObject(parent), videoWidget_(videoWidget), originalParent_(originalParent), controlBar_(controlBar)
{
    videoWidget_->installEventFilter(this);
    videoWidget_->setMouseTracking(true);
    hideTimer_ = new QTimer(this);
    hideTimer_->setSingleShot(true);
    // 连接鼠标移动信号
    connect(hideTimer_, &QTimer::timeout, this, &ControlBarManager::hideControlBar);
    // connect(videoWidget_, &VideoWidget::mouseMoved, this, &ControlBarManager::onMouseMove);
}

void ControlBarManager::init()
{

    controlBar_->setVisible(true);
    // controlBar_->setAttribute(Qt::WA_TranslucentBackground);
    controlBar_->setStyleSheet("QWidget { background-color: rgba(0, 0, 0, 40); }");
    repositionControlBar();
}

void ControlBarManager::onMouseMove()
{
    if (!originalParent_->isFullScreen()) return;
    showControlBar();
    hideTimer_->start(3000);
}

void ControlBarManager::onResize()
{
    repositionControlBar();
}

void ControlBarManager::enterFullScreen()
{
    controlBar_->setParent(videoWidget_);
    controlBar_->raise();
    controlBar_->setVisible(true);
    controlBar_->setWindowOpacity(0.5);
    controlBarVisible_ = true;
    repositionControlBar();
    hideTimer_->start(3000);


}

void ControlBarManager::exitFullScreen()
{
    controlBar_->setParent(originalParent_);
    controlBar_->raise();
    controlBar_->setVisible(true);
    controlBar_->setWindowOpacity(1.0);  // 在非全屏模式下，控制栏完全不透明
    controlBarVisible_ = true;
    hideTimer_->stop();  // 停止定时器


}

void ControlBarManager::hideControlBar()
{
    if (controlBarVisible_) {
        QPropertyAnimation* anim = new QPropertyAnimation(controlBar_, "windowOpacity",this);
        anim->setDuration(300);
        anim->setStartValue(1.0);
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, [=]() {
            controlBar_->setVisible(false);
            controlBarVisible_ = false;
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

}


void ControlBarManager::showControlBar()
{
    if (!controlBarVisible_) {
        controlBar_->setVisible(true);
        controlBarVisible_ = true;
        QPropertyAnimation* anim = new QPropertyAnimation(controlBar_, "windowOpacity",this);
        anim->setDuration(300);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->start(QAbstractAnimation::DeleteWhenStopped);

    }
}

void ControlBarManager::repositionControlBar()
{
    int w = originalParent_->width();
    int h = originalParent_->height();
    int barHeight = controlBar_->sizeHint().height();
    controlBar_->setGeometry(0, h - barHeight, w, barHeight);
}

