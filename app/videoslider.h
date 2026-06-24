#ifndef VIDEOSLIDER_H
#define VIDEOSLIDER_H
#include <QSlider>
#include <QElapsedTimer>

class VideoSlider : public QSlider {
    Q_OBJECT
public:
    explicit VideoSlider(QWidget *parent = nullptr);
    void changeValue(int n);


signals:
    /** 点击事件 */
    void clicked(VideoSlider *slider);
    void preview(int seektime, int x);
    void mouseleave();


private:
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void leaveEvent(QEvent *ev) override;
    void keyPressEvent(QKeyEvent *ev) override;

    clock_t start;
    int x;
    QElapsedTimer timer_;
};

#endif // VIDEOSLIDER_H

