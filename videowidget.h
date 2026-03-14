#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QImage>
#include "videoplayer.h"
#include <QTimer>
#include <QPropertyAnimation>

/*
 * 显示（渲染）视频
 */
class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();
    void setFullscreenMode(bool isFullscreen);
    void setControlBar(QWidget *controlBar);
    void adjustControlBarGeometry();        //调整控制栏尺寸

public slots:
    void onPlayerFrameDecoded(VideoPlayer *player,uint8_t *data,VideoPlayer::VideoSwsSpec &spec);
    void onPlayerStateChanged(VideoPlayer *player);
    void handleSizeModeChanged(int mode);   //切换画面石村
    void setBrightness(int value);          //设置亮度值

private:

    QImage *_image = nullptr;
    QRect _rect;
    int mode_ = -2;                     //画面尺寸模式

    QTimer hideTimer_;                  //控制栏定时器
    bool isFullScreen_ = false;
    QWidget *controlBar_ = nullptr;     //控制栏
    int brightness_ = 50;               //亮度值:默认50为正常亮度，范围0-100

    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void freeImage();



signals:

};

#endif // VIDEOWIDGET_H
