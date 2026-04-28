/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "render/openglrenderer.h"
#include "videoslider.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QGridLayout *gridLayout;
    QWidget *playerPage;
    QHBoxLayout *horizontalLayout_4;
    OpenGLRenderer *videoWidget;
    QGridLayout *gridLayout_2;
    QHBoxLayout *horizontalLayout_5;
    QSpacerItem *horizontalSpacer_4;
    QPushButton *openListBtn;
    QVBoxLayout *verticalLayout_3;
    QLabel *speedLabel;
    QLabel *infoLabel;
    QSpacerItem *verticalSpacer;
    QPushButton *logoBtn;
    QSpacerItem *verticalSpacer_2;
    QSpacerItem *horizontalSpacer_3;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QPushButton *addFileBtn;
    QPushButton *addDirBtn;
    QPushButton *clearListBtn;
    QListWidget *fileList;
    QWidget *controlBarContainer;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *processSLiderBar;
    QLabel *nowTimeLabel;
    VideoSlider *progressSlider;
    QLabel *allTimeLabel;
    QHBoxLayout *controlButtonBar;
    QPushButton *openFileBtn;
    QPushButton *rtspButton;
    QPushButton *settingBtn;
    QSpacerItem *horizontalSpacer;
    QPushButton *preVideoBtn;
    QPushButton *back3sBtn;
    QPushButton *startBtn;
    QPushButton *forward3sBtn;
    QPushButton *nextVideoBtn;
    QPushButton *stopBtn;
    QSpacerItem *horizontalSpacer_2;
    QComboBox *mutipleSPeed;
    QPushButton *volumeBtn;
    VideoSlider *volumeSlider;
    QLabel *volumeLabel;
    QPushButton *screenShotBtn;
    QPushButton *fullScreenBtn;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(858, 635);
        MainWindow->setStyleSheet(QString::fromUtf8("QWidget{\n"
"	background:black;\n"
"}"));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        gridLayout = new QGridLayout(centralwidget);
        gridLayout->setObjectName("gridLayout");
        gridLayout->setContentsMargins(0, 0, 0, 0);
        playerPage = new QWidget(centralwidget);
        playerPage->setObjectName("playerPage");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(playerPage->sizePolicy().hasHeightForWidth());
        playerPage->setSizePolicy(sizePolicy);
        horizontalLayout_4 = new QHBoxLayout(playerPage);
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        videoWidget = new OpenGLRenderer(playerPage);
        videoWidget->setObjectName("videoWidget");
        sizePolicy.setHeightForWidth(videoWidget->sizePolicy().hasHeightForWidth());
        videoWidget->setSizePolicy(sizePolicy);
        gridLayout_2 = new QGridLayout(videoWidget);
        gridLayout_2->setObjectName("gridLayout_2");
        gridLayout_2->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName("horizontalLayout_5");
        horizontalSpacer_4 = new QSpacerItem(278, 28, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_5->addItem(horizontalSpacer_4);

        openListBtn = new QPushButton(videoWidget);
        openListBtn->setObjectName("openListBtn");
        openListBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	background-color: rgba(0, 0, 0, 160);  /* \345\215\212\351\200\217\346\230\216\351\273\221 */\n"
"	max-height:50px;\n"
"	max-width:23px;\n"
"	min-height:50px;\n"
"	min-width:23px;\n"
"	border:none;\n"
"	border-radius: 5px;\n"
"	margin-right:5px;\n"
"}"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/SmartPlayer-icon/right_arrow.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        openListBtn->setIcon(icon);
        openListBtn->setIconSize(QSize(20, 20));

        horizontalLayout_5->addWidget(openListBtn);


        gridLayout_2->addLayout(horizontalLayout_5, 0, 2, 1, 1);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName("verticalLayout_3");
        speedLabel = new QLabel(videoWidget);
        speedLabel->setObjectName("speedLabel");
        speedLabel->setEnabled(false);
        speedLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	color:white;\n"
"	min-width: 100px;\n"
"	min-height: 30px;\n"
"	max-width: 100px;\n"
"	max-height: 30px;\n"
"	font-size: 13px;\n"
"	background-color: rgba(0, 0, 0, 150); \n"
"	border-radius: 5px;\n"
"   	text-align: center;\n"
"    	alignment: center;\n"
"	margin-top:5px;\n"
"\n"
"}"));

        verticalLayout_3->addWidget(speedLabel);

        infoLabel = new QLabel(videoWidget);
        infoLabel->setObjectName("infoLabel");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(infoLabel->sizePolicy().hasHeightForWidth());
        infoLabel->setSizePolicy(sizePolicy1);
        infoLabel->setMinimumSize(QSize(0, 39));
        infoLabel->setMaximumSize(QSize(624, 39));
        infoLabel->setStyleSheet(QString::fromUtf8("QLabel {\n"
"        background-color: rgba(0, 0, 0, 160);  /* \345\215\212\351\200\217\346\230\216\351\273\221 */\n"
"	max-height: 18px;\n"
"	min-height:18px;\n"
"	max-width: 600px;\n"
"        color: white;                          /* \351\253\230\345\257\271\346\257\224\345\255\227\344\275\223 */\n"
"        border-radius: 8px;\n"
"        padding: 8px 12px;\n"
"        font-size: 12px;\n"
"        font-weight: bold;\n"
"	margin-top:5px;\n"
" }"));

        verticalLayout_3->addWidget(infoLabel);

        verticalSpacer = new QSpacerItem(20, 198, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_3->addItem(verticalSpacer);

        logoBtn = new QPushButton(videoWidget);
        logoBtn->setObjectName("logoBtn");
        logoBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:50px;\n"
"	max-width:50px;\n"
"	min-height:50px;\n"
"	min-width:50px;\n"
"	border:none;\n"
"	background-color: transparent;\n"
"}"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/SmartPlayer-icon/logo.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        logoBtn->setIcon(icon1);
        logoBtn->setIconSize(QSize(50, 50));

        verticalLayout_3->addWidget(logoBtn);

        verticalSpacer_2 = new QSpacerItem(20, 248, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_3->addItem(verticalSpacer_2);


        gridLayout_2->addLayout(verticalLayout_3, 0, 1, 1, 1);

        horizontalSpacer_3 = new QSpacerItem(345, 460, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        gridLayout_2->addItem(horizontalSpacer_3, 0, 0, 1, 1);


        horizontalLayout_4->addWidget(videoWidget);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(0);
        horizontalLayout->setObjectName("horizontalLayout");
        addFileBtn = new QPushButton(playerPage);
        addFileBtn->setObjectName("addFileBtn");
        addFileBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        addFileBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:25px;\n"
"	max-width:25px;\n"
"	min-height:25px;\n"
"	min-width:25px;\n"
"	border:none;\n"
"}"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/SmartPlayer-icon/add_file.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        addFileBtn->setIcon(icon2);
        addFileBtn->setIconSize(QSize(20, 20));

        horizontalLayout->addWidget(addFileBtn);

        addDirBtn = new QPushButton(playerPage);
        addDirBtn->setObjectName("addDirBtn");
        addDirBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        addDirBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:25px;\n"
"	max-width:25px;\n"
"	min-height:25px;\n"
"	min-width:25px;\n"
"	border:none;\n"
"}"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/SmartPlayer-icon/add_dir.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        addDirBtn->setIcon(icon3);
        addDirBtn->setIconSize(QSize(20, 20));

        horizontalLayout->addWidget(addDirBtn);

        clearListBtn = new QPushButton(playerPage);
        clearListBtn->setObjectName("clearListBtn");
        clearListBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        clearListBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:25px;\n"
"	max-width:25px;\n"
"	min-height:25px;\n"
"	min-width:25px;\n"
"	border:none;\n"
"}"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/SmartPlayer-icon/clear_list.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        clearListBtn->setIcon(icon4);
        clearListBtn->setIconSize(QSize(20, 20));

        horizontalLayout->addWidget(clearListBtn);


        verticalLayout->addLayout(horizontalLayout);

        fileList = new QListWidget(playerPage);
        fileList->setObjectName("fileList");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(10);
        sizePolicy2.setHeightForWidth(fileList->sizePolicy().hasHeightForWidth());
        fileList->setSizePolicy(sizePolicy2);
        fileList->setMinimumSize(QSize(290, 0));
        fileList->setMaximumSize(QSize(290, 16777215));
        fileList->setStyleSheet(QString::fromUtf8("QListWidget{\n"
"	border:none;\n"
"}\n"
"QListWidget::item:selected {\n"
"    background: transparent;\n"
"}\n"
" QListWidget::item:hover {\n"
"        background: rgba(100, 100, 100,100);  /* \345\215\212\351\200\217\346\230\216\347\201\260\350\211\262\346\202\254\346\265\256 */\n"
"    }\n"
"QScrollBar:horizontal {\n"
"    height: 12px; /* \346\273\232\345\212\250\346\235\241\351\253\230\345\272\246 */\n"
"    background: #f0f0f0; /* \346\273\221\346\247\275\350\203\214\346\231\257 */\n"
"}\n"
"\n"
"QScrollBar::handle:horizontal {\n"
"    background: #c1c1c1; /* \346\273\221\345\235\227\351\242\234\350\211\262 */\n"
"    min-width: 20px; /* \346\273\221\345\235\227\346\234\200\345\260\217\345\256\275\345\272\246 */\n"
"    border-radius: 6px; /* \346\273\221\345\235\227\345\234\206\350\247\222 */\n"
"}\n"
"\n"
"QScrollBar::handle:horizontal:hover {\n"
"    background: #a8a8a8; /* \346\273\221\345\235\227\346\202\254\346\265\256\346\227\266\351\242\234\350\211\262 */\n"
"}\n"
"\n"
"QScrollBar::add-line:horizo"
                        "ntal, QScrollBar::sub-line:horizontal {\n"
"    background: none; /* \351\232\220\350\227\217\345\267\246\345\217\263\347\256\255\345\244\264 */\n"
"}\n"
"\n"
"QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {\n"
"    background: none; /* \346\273\221\346\247\275\346\234\252\350\242\253\346\273\221\345\235\227\350\246\206\347\233\226\347\232\204\351\203\250\345\210\206 */\n"
"}"));
        fileList->setWordWrap(true);

        verticalLayout->addWidget(fileList);


        horizontalLayout_4->addLayout(verticalLayout);


        gridLayout->addWidget(playerPage, 0, 0, 1, 1);

        controlBarContainer = new QWidget(centralwidget);
        controlBarContainer->setObjectName("controlBarContainer");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(controlBarContainer->sizePolicy().hasHeightForWidth());
        controlBarContainer->setSizePolicy(sizePolicy3);
        controlBarContainer->setMinimumSize(QSize(0, 80));
        verticalLayout_2 = new QVBoxLayout(controlBarContainer);
        verticalLayout_2->setObjectName("verticalLayout_2");
        processSLiderBar = new QHBoxLayout();
        processSLiderBar->setObjectName("processSLiderBar");
        nowTimeLabel = new QLabel(controlBarContainer);
        nowTimeLabel->setObjectName("nowTimeLabel");
        nowTimeLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	color:white;\n"
"	background-color: transparent;\n"
"border-radius:5px;\n"
"}"));

        processSLiderBar->addWidget(nowTimeLabel);

        progressSlider = new VideoSlider(controlBarContainer);
        progressSlider->setObjectName("progressSlider");
        progressSlider->setStyleSheet(QString::fromUtf8("QSlider{\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}\n"
"\n"
"QSlider::groove:horizontal {\n"
"    background-color: #ccc;\n"
"    height: 4px;\n"
"    border-radius: 2px;\n"
"}\n"
"\n"
"QSlider::handle:horizontal {\n"
"    image: url(:/SmartPlayer-icon/tv.png);\n"
"    width: 20px;\n"
"    height: 20px;\n"
"    margin: -10px -3px; /* \344\275\277\346\211\213\346\237\204\345\261\205\344\270\255 */\n"
"    border-radius: 5px;\n"
"}\n"
"QSlider::sub-page:horizontal {\n"
"    background-color:  rgb(232, 88, 158); /* \345\267\262\346\273\221\345\212\250\351\203\250\345\210\206\347\232\204\351\242\234\350\211\262 */\n"
"	border-radius: 5px;\n"
"}\n"
"\n"
"QSlider::add-page:horizontal {\n"
"         background-color: white;\n"
"         border-radius: 5px;\n"
"}"));
        progressSlider->setOrientation(Qt::Orientation::Horizontal);

        processSLiderBar->addWidget(progressSlider);

        allTimeLabel = new QLabel(controlBarContainer);
        allTimeLabel->setObjectName("allTimeLabel");
        allTimeLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	color:white;\n"
"background-color:transparent;\n"
"border-radius:5px;\n"
"}"));

        processSLiderBar->addWidget(allTimeLabel);


        verticalLayout_2->addLayout(processSLiderBar);

        controlButtonBar = new QHBoxLayout();
        controlButtonBar->setObjectName("controlButtonBar");
        openFileBtn = new QPushButton(controlBarContainer);
        openFileBtn->setObjectName("openFileBtn");
        openFileBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        openFileBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"	background-color: transparent;\n"
"	border-radius:5px;\n"
"}"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/SmartPlayer-icon/folder.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        openFileBtn->setIcon(icon5);
        openFileBtn->setIconSize(QSize(25, 25));

        controlButtonBar->addWidget(openFileBtn);

        rtspButton = new QPushButton(controlBarContainer);
        rtspButton->setObjectName("rtspButton");
        rtspButton->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"	border-radius:5px;\n"
"	background-color: transparent;\n"
"}"));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/SmartPlayer-icon/rtsp.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        rtspButton->setIcon(icon6);
        rtspButton->setIconSize(QSize(25, 25));

        controlButtonBar->addWidget(rtspButton);

        settingBtn = new QPushButton(controlBarContainer);
        settingBtn->setObjectName("settingBtn");
        settingBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}\n"
""));
        QIcon icon7;
        icon7.addFile(QString::fromUtf8(":/SmartPlayer-icon/setting.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        settingBtn->setIcon(icon7);
        settingBtn->setIconSize(QSize(24, 24));

        controlButtonBar->addWidget(settingBtn);

        horizontalSpacer = new QSpacerItem(278, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        controlButtonBar->addItem(horizontalSpacer);

        preVideoBtn = new QPushButton(controlBarContainer);
        preVideoBtn->setObjectName("preVideoBtn");
        preVideoBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        preVideoBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));
        QIcon icon8;
        icon8.addFile(QString::fromUtf8(":/SmartPlayer-icon/last_video.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        preVideoBtn->setIcon(icon8);
        preVideoBtn->setIconSize(QSize(20, 20));

        controlButtonBar->addWidget(preVideoBtn);

        back3sBtn = new QPushButton(controlBarContainer);
        back3sBtn->setObjectName("back3sBtn");
        back3sBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        back3sBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));
        QIcon icon9;
        icon9.addFile(QString::fromUtf8(":/SmartPlayer-icon/back.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        back3sBtn->setIcon(icon9);
        back3sBtn->setIconSize(QSize(25, 25));

        controlButtonBar->addWidget(back3sBtn);

        startBtn = new QPushButton(controlBarContainer);
        startBtn->setObjectName("startBtn");
        startBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        startBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));
        QIcon icon10;
        icon10.addFile(QString::fromUtf8(":/SmartPlayer-icon/start.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        startBtn->setIcon(icon10);
        startBtn->setIconSize(QSize(30, 30));

        controlButtonBar->addWidget(startBtn);

        forward3sBtn = new QPushButton(controlBarContainer);
        forward3sBtn->setObjectName("forward3sBtn");
        forward3sBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        forward3sBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));
        QIcon icon11;
        icon11.addFile(QString::fromUtf8(":/SmartPlayer-icon/forward.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        forward3sBtn->setIcon(icon11);
        forward3sBtn->setIconSize(QSize(25, 25));

        controlButtonBar->addWidget(forward3sBtn);

        nextVideoBtn = new QPushButton(controlBarContainer);
        nextVideoBtn->setObjectName("nextVideoBtn");
        nextVideoBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        nextVideoBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));
        QIcon icon12;
        icon12.addFile(QString::fromUtf8(":/SmartPlayer-icon/next_video.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        nextVideoBtn->setIcon(icon12);
        nextVideoBtn->setIconSize(QSize(20, 20));

        controlButtonBar->addWidget(nextVideoBtn);

        stopBtn = new QPushButton(controlBarContainer);
        stopBtn->setObjectName("stopBtn");
        stopBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        stopBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}\n"
""));
        QIcon icon13;
        icon13.addFile(QString::fromUtf8(":/SmartPlayer-icon/stop.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        stopBtn->setIcon(icon13);
        stopBtn->setIconSize(QSize(20, 20));

        controlButtonBar->addWidget(stopBtn);

        horizontalSpacer_2 = new QSpacerItem(118, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        controlButtonBar->addItem(horizontalSpacer_2);

        mutipleSPeed = new QComboBox(controlBarContainer);
        mutipleSPeed->addItem(QString());
        mutipleSPeed->addItem(QString());
        mutipleSPeed->addItem(QString());
        mutipleSPeed->addItem(QString());
        mutipleSPeed->setObjectName("mutipleSPeed");
        mutipleSPeed->setMinimumSize(QSize(50, 20));
        mutipleSPeed->setMaximumSize(QSize(50, 20));
        mutipleSPeed->setStyleSheet(QString::fromUtf8("QComboBox {\n"
"    border:none;\n"
"    border-radius: 8px;\n"
"    padding: 5px;\n"
"    background-color: transparent;\n"
"	color:white;\n"
"}\n"
"\n"
"\n"
"QComboBox QAbstractItemView {\n"
"    color: white; /* \344\277\256\346\224\271\344\270\213\346\213\211\346\241\206\345\206\205\345\255\227\344\275\223\351\242\234\350\211\262 */\n"
"    background-color: black; /* \344\277\256\346\224\271\344\270\213\346\213\211\346\241\206\350\203\214\346\231\257\351\242\234\350\211\262 */\n"
"    selection-background-color: blue; /* \344\277\256\346\224\271\351\200\211\344\270\255\351\241\271\347\232\204\350\203\214\346\231\257\351\242\234\350\211\262 */\n"
"    selection-color: white; /* \344\277\256\346\224\271\351\200\211\344\270\255\351\241\271\347\232\204\345\255\227\344\275\223\351\242\234\350\211\262 */\n"
"}"));

        controlButtonBar->addWidget(mutipleSPeed);

        volumeBtn = new QPushButton(controlBarContainer);
        volumeBtn->setObjectName("volumeBtn");
        volumeBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        volumeBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));
        QIcon icon14;
        icon14.addFile(QString::fromUtf8(":/SmartPlayer-icon/volume.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        volumeBtn->setIcon(icon14);
        volumeBtn->setIconSize(QSize(20, 20));

        controlButtonBar->addWidget(volumeBtn);

        volumeSlider = new VideoSlider(controlBarContainer);
        volumeSlider->setObjectName("volumeSlider");
        volumeSlider->setMinimumSize(QSize(100, 0));
        volumeSlider->setMaximumSize(QSize(100, 16777215));
        volumeSlider->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        volumeSlider->setStyleSheet(QString::fromUtf8("QSlider{\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}\n"
"\n"
"QSlider::groove:horizontal {\n"
"    background-color: #ccc;\n"
"    height: 3px;\n"
"    border-radius: 2px;\n"
"}\n"
"\n"
"QSlider::handle:horizontal {\n"
"    background-color:rgb(66, 148, 255);\n"
"    width: 12px;\n"
"    height: 12px;\n"
"    margin: -4px 0; /* \344\275\277\346\211\213\346\237\204\345\261\205\344\270\255 */\n"
"    border-radius: 5px;\n"
"}\n"
"QSlider::sub-page:horizontal {\n"
"    background-color: rgb(66, 148, 255); /* \345\267\262\346\273\221\345\212\250\351\203\250\345\210\206\347\232\204\351\242\234\350\211\262 */\n"
"}"));
        volumeSlider->setOrientation(Qt::Orientation::Horizontal);

        controlButtonBar->addWidget(volumeSlider);

        volumeLabel = new QLabel(controlBarContainer);
        volumeLabel->setObjectName("volumeLabel");
        volumeLabel->setMinimumSize(QSize(25, 25));
        volumeLabel->setMaximumSize(QSize(25, 25));
        volumeLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	color:white;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));

        controlButtonBar->addWidget(volumeLabel);

        screenShotBtn = new QPushButton(controlBarContainer);
        screenShotBtn->setObjectName("screenShotBtn");
        screenShotBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        screenShotBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));
        QIcon icon15;
        icon15.addFile(QString::fromUtf8(":/SmartPlayer-icon/take_shot.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        screenShotBtn->setIcon(icon15);
        screenShotBtn->setIconSize(QSize(26, 26));

        controlButtonBar->addWidget(screenShotBtn);

        fullScreenBtn = new QPushButton(controlBarContainer);
        fullScreenBtn->setObjectName("fullScreenBtn");
        fullScreenBtn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        fullScreenBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:35px;\n"
"	max-width:35px;\n"
"	min-height:35px;\n"
"	min-width:35px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));
        QIcon icon16;
        icon16.addFile(QString::fromUtf8(":/SmartPlayer-icon/full_screen.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        fullScreenBtn->setIcon(icon16);
        fullScreenBtn->setIconSize(QSize(23, 23));

        controlButtonBar->addWidget(fullScreenBtn);


        verticalLayout_2->addLayout(controlButtonBar);


        gridLayout->addWidget(controlBarContainer, 1, 0, 1, 1);

        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);

        mutipleSPeed->setCurrentIndex(2);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        openListBtn->setText(QString());
        speedLabel->setText(QCoreApplication::translate("MainWindow", "    2\345\200\215\351\200\237\346\222\255\346\224\276\344\270\255", nullptr));
        infoLabel->setText(QString());
        logoBtn->setText(QString());
#if QT_CONFIG(tooltip)
        addFileBtn->setToolTip(QCoreApplication::translate("MainWindow", "\346\267\273\345\212\240\346\226\207\344\273\266", nullptr));
#endif // QT_CONFIG(tooltip)
        addFileBtn->setText(QString());
#if QT_CONFIG(tooltip)
        addDirBtn->setToolTip(QCoreApplication::translate("MainWindow", "\346\267\273\345\212\240\346\226\207\344\273\266\345\244\271", nullptr));
#endif // QT_CONFIG(tooltip)
        addDirBtn->setText(QString());
#if QT_CONFIG(tooltip)
        clearListBtn->setToolTip(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272\345\210\227\350\241\250", nullptr));
#endif // QT_CONFIG(tooltip)
        clearListBtn->setText(QString());
        nowTimeLabel->setText(QCoreApplication::translate("MainWindow", "00:00:00", nullptr));
        allTimeLabel->setText(QCoreApplication::translate("MainWindow", "00:00:00", nullptr));
#if QT_CONFIG(tooltip)
        openFileBtn->setToolTip(QCoreApplication::translate("MainWindow", "\346\211\223\345\274\200\346\226\207\344\273\266", nullptr));
#endif // QT_CONFIG(tooltip)
        openFileBtn->setText(QString());
#if QT_CONFIG(tooltip)
        rtspButton->setToolTip(QCoreApplication::translate("MainWindow", "\346\211\223\345\274\200\347\275\221\347\273\234\346\265\201", nullptr));
#endif // QT_CONFIG(tooltip)
        rtspButton->setText(QString());
#if QT_CONFIG(tooltip)
        settingBtn->setToolTip(QCoreApplication::translate("MainWindow", "\350\256\276\347\275\256", nullptr));
#endif // QT_CONFIG(tooltip)
        settingBtn->setText(QString());
#if QT_CONFIG(tooltip)
        preVideoBtn->setToolTip(QCoreApplication::translate("MainWindow", "\344\270\212\344\270\200\344\270\252", nullptr));
#endif // QT_CONFIG(tooltip)
        preVideoBtn->setText(QString());
#if QT_CONFIG(tooltip)
        back3sBtn->setToolTip(QCoreApplication::translate("MainWindow", "\345\220\216\351\200\20010s", nullptr));
#endif // QT_CONFIG(tooltip)
        back3sBtn->setText(QString());
#if QT_CONFIG(tooltip)
        startBtn->setToolTip(QCoreApplication::translate("MainWindow", "\346\222\255\346\224\276", nullptr));
#endif // QT_CONFIG(tooltip)
        startBtn->setText(QString());
#if QT_CONFIG(tooltip)
        forward3sBtn->setToolTip(QCoreApplication::translate("MainWindow", "\345\277\253\350\277\23310s", nullptr));
#endif // QT_CONFIG(tooltip)
        forward3sBtn->setText(QString());
#if QT_CONFIG(tooltip)
        nextVideoBtn->setToolTip(QCoreApplication::translate("MainWindow", "\344\270\213\344\270\200\344\270\252", nullptr));
#endif // QT_CONFIG(tooltip)
        nextVideoBtn->setText(QString());
#if QT_CONFIG(tooltip)
        stopBtn->setToolTip(QCoreApplication::translate("MainWindow", "\345\201\234\346\255\242", nullptr));
#endif // QT_CONFIG(tooltip)
        stopBtn->setText(QString());
        mutipleSPeed->setItemText(0, QCoreApplication::translate("MainWindow", "2.0x", nullptr));
        mutipleSPeed->setItemText(1, QCoreApplication::translate("MainWindow", "1.5x", nullptr));
        mutipleSPeed->setItemText(2, QCoreApplication::translate("MainWindow", "1.0x", nullptr));
        mutipleSPeed->setItemText(3, QCoreApplication::translate("MainWindow", "0.5x", nullptr));

        mutipleSPeed->setCurrentText(QCoreApplication::translate("MainWindow", "1.0x", nullptr));
        volumeBtn->setText(QString());
        volumeLabel->setText(QCoreApplication::translate("MainWindow", "50", nullptr));
#if QT_CONFIG(tooltip)
        screenShotBtn->setToolTip(QCoreApplication::translate("MainWindow", "\350\247\206\351\242\221\346\210\252\345\233\276", nullptr));
#endif // QT_CONFIG(tooltip)
        screenShotBtn->setText(QString());
#if QT_CONFIG(tooltip)
        fullScreenBtn->setToolTip(QCoreApplication::translate("MainWindow", "\345\205\250\345\261\217", nullptr));
#endif // QT_CONFIG(tooltip)
        fullScreenBtn->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
