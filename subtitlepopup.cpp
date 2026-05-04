#include "subtitlepopup.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>

SubtitlePopup::SubtitlePopup(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);

    // ===== 第一行：实时字幕 =====
    auto row1 = new QHBoxLayout;
    QLabel *label1 = new QLabel("实时字幕");

    realtimeBtn_ = new QPushButton;
    realtimeBtn_->setCheckable(true);
    realtimeBtn_->setFixedSize(40, 30);
    realtimeBtn_->setIcon(QIcon(":/SmartPlayer-icon/uncheck.png"));
    realtimeBtn_->setIconSize(QSize(40, 30));
    realtimeBtn_->setFlat(true);
    realtimeBtn_->setFocusPolicy(Qt::NoFocus);
    connect(realtimeBtn_, &QPushButton::toggled, this, [=](bool checked){
        realtimeBtn_->setIcon(QIcon(checked ?
                                        ":/SmartPlayer-icon/checked.png" :
                                        ":/SmartPlayer-icon/uncheck.png"));
    });

    row1->addWidget(label1);
    row1->addStretch();
    row1->addWidget(realtimeBtn_);

    // ===== 第二行：中英翻译 =====
    auto row2 = new QHBoxLayout;
    QLabel *label2 = new QLabel("中英翻译");

    translateBtn_ = new QPushButton;
    translateBtn_->setCheckable(true);
    translateBtn_->setFixedSize(40, 30);
    translateBtn_->setIcon(QIcon(":/SmartPlayer-icon/uncheck.png"));
    translateBtn_->setIconSize(QSize(40, 30));
    translateBtn_->setFlat(true);
    translateBtn_->setFocusPolicy(Qt::NoFocus);
    connect(translateBtn_, &QPushButton::toggled, this, [=](bool checked){
        translateBtn_->setIcon(QIcon(checked ?
                                         ":/SmartPlayer-icon/checked.png" :
                                         ":/SmartPlayer-icon/uncheck.png"));
    });

    row2->addWidget(label2);
    row2->addStretch();
    row2->addWidget(translateBtn_);

    mainLayout->addLayout(row1);
    mainLayout->addLayout(row2);

    // ===== 样式 =====
    setStyleSheet(R"(
        QWidget {
            background-color: #2b2b2b;
            border: 1px solid #555;
            border-radius: 8px;
            color: white;
        }
        QLabel {
            font-size: 13px;
            border:none;
        }
        QPushButton {
            border: none;
        }
    )");
}
