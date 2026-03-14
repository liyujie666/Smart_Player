#include "videowidget.h"
#include <QDebug>
#include <QPainter>
#include <QGuiApplication>
#include <QScreen>
VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent) {
    // 设置背景色
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet("background: black");
    setMouseTracking(true);
    hideTimer_.setInterval(3000);
    connect(&hideTimer_,&QTimer::timeout,[this](){
        if(isFullScreen_ && controlBar_){
            QPoint globalCursorPos = QCursor::pos();  // 鼠标全局坐标
            QPoint cursorPosInControlBar = controlBar_->mapFromGlobal(globalCursorPos); // 转为controlBar坐标系

            if (!controlBar_->rect().contains(cursorPosInControlBar)) {
                controlBar_->hide();
                setCursor(Qt::BlankCursor);
            } else {
                // 鼠标还在 control bar 上，不隐藏，重启计时器
                hideTimer_.start();
            }
        }
    });
}

VideoWidget::~VideoWidget() {
    freeImage();
}

void VideoWidget::setFullscreenMode(bool isFullscreen)
{
    isFullScreen_ = isFullscreen;
    hideTimer_.stop();

    if(controlBar_){
        controlBar_->setVisible(!isFullScreen_);
        if(isFullscreen){
            controlBar_->setParent(this);
            controlBar_->raise();
            controlBar_->setStyleSheet(R"(
                                            QWidget {
                                                background: qlineargradient(
                                                    x1:0, y1:0, x2:0, y2:1,
                                                    stop:0 rgba(0,0,0,0),
                                                    stop:1 rgba(0,0,0,180)
                                                );
                                            }
                                        )");
            controlBar_->show();
            adjustControlBarGeometry();
            hideTimer_.start();
        }
    }
}

void VideoWidget::setControlBar(QWidget *controlBar)
{
    controlBar_ = controlBar;
}

void VideoWidget::setBrightness(int value)
{
    brightness_ = value;
    update();
}

void VideoWidget::adjustControlBarGeometry()
{
    if (controlBar_) {
        const int barHeight = 80; // 根据实际高度调整
        controlBar_->setGeometry(
            0,
            height() - barHeight,
            width(),
            barHeight
            );
    }
}



void VideoWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (isFullScreen_) {
        adjustControlBarGeometry();

    }
}
void VideoWidget::paintEvent(QPaintEvent *event) {
    if (!_image) return;

    // 将图片绘制到当前组件上
    QPainter painter(this);
    painter.drawImage(_rect, *_image);
    //亮度滤镜处理
    if(brightness_ != 50){
        int diff = std::abs(brightness_ - 50) * 2; // 控制叠加透明度（范围 0~200）
        QColor overlayColor = (brightness_ > 50) ? QColor(255, 255, 255, diff)
                                                 : QColor(0, 0, 0, diff);
        painter.fillRect(rect(), overlayColor);
    }
}

void VideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    if(isFullScreen_ && controlBar_){
        if(!controlBar_->isVisible()){
            controlBar_->show();
        }
        // 显示鼠标
        unsetCursor();
        hideTimer_.start();
    }
    QWidget::mouseMoveEvent(event);
}

void VideoWidget::onPlayerStateChanged(VideoPlayer *player) {
    if (player->getState() != VideoPlayer::Stopped) return;

    freeImage();
    update();
}



void VideoWidget::handleSizeModeChanged(int mode)
{
    mode_ = mode;
    qDebug() << "当前模式为：" << mode_;
}

void VideoWidget::onPlayerFrameDecoded(VideoPlayer *player,uint8_t *data,VideoPlayer::VideoSwsSpec &spec) {
    if (player->getState() == VideoPlayer::Stopped) return;


    // 释放之前的图片
    freeImage();

    // qDebug() << "widget size:" << width() << height();
    // qDebug() << "screen size:" << QGuiApplication::primaryScreen()->size();
    // qDebug() << "devicePixelRatio:" << devicePixelRatio();


    // 创建新的图片
    if (data != nullptr) {
        _image=new QImage(data,
                            spec.width, spec.height,
                            QImage::Format_RGB888);
        // 计算最终的尺寸
        // 组件的尺寸
        int w=width();
        int h=height();

        // 计算rect
        int dx=0;
        int dy=0;
        int dw=spec.width;
        int dh=spec.height;

        float widthRatio;
        float heightRatio;
        float scaleRatio;

        switch(mode_){
            case -2:     //默认效果
                widthRatio = (float)w / dw;
                heightRatio = (float)h / dh;

                // 选择合适的比例来确保铺满全屏
                scaleRatio = std::max(widthRatio, heightRatio);

                dw = static_cast<int>(dw * scaleRatio);
                dh = static_cast<int>(dh * scaleRatio);

                // 计算居中位置
                dx = (w - dw) / 2;
                dy = (h - dh) / 2;

                break;
            case -3:     //拉伸效果
                dw = w;
                dh = h;

                dx = 0;
                dy = 0;
                break;
            case -4:     //填充效果
                if (dw>w||dh>h) { // 缩放
                    if (dw*h>w*dh) { // 视频的宽高比 > 播放器的宽高比
                        dh=w*dh/dw;
                        dw=w;
                    } else {
                        dw=h*dw/dh;
                        dh=h;
                    }
                }
                dx=(w - dw) >> 1;
                dy=(h - dh) >> 1;
                break;
            case -5:     // 裁剪铺满屏幕（不变形，但裁掉边缘）
                if (dw * h < w * dh) {
                    dh = w * dh / dw;
                    dw = w;
                } else {
                    dw = h * dw / dh;
                    dh = h;
                }
                dx = (w - dw) >> 1;
                dy = (h - dh) >> 1;
                break;
            default:
                break;
        }

        _rect=QRect(dx, dy, dw, dh);
    }
    update();
}

void VideoWidget::freeImage() {
    if (_image) {
        av_free(_image->bits());
        delete _image;
        _image=nullptr;
    }
}


