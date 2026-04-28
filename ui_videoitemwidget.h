/********************************************************************************
** Form generated from reading UI file 'videoitemwidget.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VIDEOITEMWIDGET_H
#define UI_VIDEOITEMWIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_VideoItemWidget
{
public:
    QGridLayout *gridLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *previewImageLabel;
    QVBoxLayout *verticalLayout;
    QLabel *fileNameLabel;
    QLabel *durationLabel;

    void setupUi(QWidget *VideoItemWidget)
    {
        if (VideoItemWidget->objectName().isEmpty())
            VideoItemWidget->setObjectName("VideoItemWidget");
        VideoItemWidget->resize(290, 85);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(VideoItemWidget->sizePolicy().hasHeightForWidth());
        VideoItemWidget->setSizePolicy(sizePolicy);
        VideoItemWidget->setMinimumSize(QSize(290, 80));
        VideoItemWidget->setMaximumSize(QSize(290, 85));
        VideoItemWidget->setStyleSheet(QString::fromUtf8(""));
        gridLayout = new QGridLayout(VideoItemWidget);
        gridLayout->setObjectName("gridLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName("horizontalLayout");
        previewImageLabel = new QLabel(VideoItemWidget);
        previewImageLabel->setObjectName("previewImageLabel");
        previewImageLabel->setMinimumSize(QSize(115, 65));
        previewImageLabel->setMaximumSize(QSize(115, 65));
        previewImageLabel->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout->addWidget(previewImageLabel);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(2, -1, -1, -1);
        fileNameLabel = new QLabel(VideoItemWidget);
        fileNameLabel->setObjectName("fileNameLabel");
        fileNameLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"color:white;\n"
"font-size:12px;\n"
"border-top-left-radius: 5px;\n"
"border-top-right-radius: 5px;\n"
"}"));

        verticalLayout->addWidget(fileNameLabel);

        durationLabel = new QLabel(VideoItemWidget);
        durationLabel->setObjectName("durationLabel");
        durationLabel->setMinimumSize(QSize(0, 15));
        durationLabel->setMaximumSize(QSize(16777215, 15));
        durationLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"color:white;\n"
"font-size:12px;\n"
"border-bottom-left-radius: 5px;\n"
"border-bottom-right-radius: 5px;\n"
"}"));

        verticalLayout->addWidget(durationLabel);


        horizontalLayout->addLayout(verticalLayout);


        gridLayout->addLayout(horizontalLayout, 0, 0, 1, 1);


        retranslateUi(VideoItemWidget);

        QMetaObject::connectSlotsByName(VideoItemWidget);
    } // setupUi

    void retranslateUi(QWidget *VideoItemWidget)
    {
        VideoItemWidget->setWindowTitle(QCoreApplication::translate("VideoItemWidget", "Form", nullptr));
        previewImageLabel->setText(QCoreApplication::translate("VideoItemWidget", "TextLabel", nullptr));
        fileNameLabel->setText(QCoreApplication::translate("VideoItemWidget", "TextLabel", nullptr));
        durationLabel->setText(QCoreApplication::translate("VideoItemWidget", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VideoItemWidget: public Ui_VideoItemWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VIDEOITEMWIDGET_H
