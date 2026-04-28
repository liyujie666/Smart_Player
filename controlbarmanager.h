// #ifndef CONTROLBARMANAGER_H
// #define CONTROLBARMANAGER_H
// #include <QObject>
// #include <QObject>
// #include <QTimer>
// #include <QMouseEvent>
// #include <QPropertyAnimation>
// #include <QWidget>
// #include <QVBoxLayout>
// #include "videowidget.h"

// class ControlBarManager : public QObject
// {
//     Q_OBJECT
// public:
//     ControlBarManager(VideoWidget* videoWiget,QWidget* originalParent,QWidget* controlBar, QObject* parent = nullptr);

//     void init();
//     void onMouseMove();
//     void onResize();
//     void enterFullScreen();
//     void exitFullScreen();

// public slots:
//     void hideControlBar();


// private:
//     void showControlBar();
//     void repositionControlBar();

//     bool controlBarVisible_ = true;
//     VideoWidget* videoWidget_;
//     QWidget* originalParent_;
//     QWidget* controlBar_;
//     QTimer* hideTimer_;
//     bool isFullScreen_ = false;
// };

// #endif // CONTROLBARMANAGER_H
