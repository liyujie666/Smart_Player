/********************************************************************************
** Form generated from reading UI file 'videoinfodialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VIDEOINFODIALOG_H
#define UI_VIDEOINFODIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_VideoInfoDialog
{
public:
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout;
    QLabel *fileNameLabel;
    QLabel *durationLabel;
    QLabel *bitRateLabel;
    QLabel *frameRateLabel;
    QLabel *nameLabel;
    QLabel *picFmtLabel;
    QLabel *aChannelsLabel;
    QLabel *aSampleRateLabel;

    void setupUi(QDialog *VideoInfoDialog)
    {
        if (VideoInfoDialog->objectName().isEmpty())
            VideoInfoDialog->setObjectName("VideoInfoDialog");
        VideoInfoDialog->resize(330, 375);
        VideoInfoDialog->setMinimumSize(QSize(330, 375));
        VideoInfoDialog->setMaximumSize(QSize(330, 375));
        VideoInfoDialog->setStyleSheet(QString::fromUtf8(""));
        layoutWidget = new QWidget(VideoInfoDialog);
        layoutWidget->setObjectName("layoutWidget");
        layoutWidget->setGeometry(QRect(10, 1, 312, 364));
        verticalLayout = new QVBoxLayout(layoutWidget);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        fileNameLabel = new QLabel(layoutWidget);
        fileNameLabel->setObjectName("fileNameLabel");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(fileNameLabel->sizePolicy().hasHeightForWidth());
        fileNameLabel->setSizePolicy(sizePolicy);
        fileNameLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-height:40px;\n"
"	max-height:40px;\n"
"	min-width: 310px;\n"
"	max-width:310px;\n"
"}"));
        fileNameLabel->setWordWrap(true);

        verticalLayout->addWidget(fileNameLabel);

        durationLabel = new QLabel(layoutWidget);
        durationLabel->setObjectName("durationLabel");
        sizePolicy.setHeightForWidth(durationLabel->sizePolicy().hasHeightForWidth());
        durationLabel->setSizePolicy(sizePolicy);
        durationLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-height:40px;\n"
"	max-height:40px;\n"
"	min-width: 310px;\n"
"	max-width:310px;\n"
"}"));
        durationLabel->setWordWrap(true);

        verticalLayout->addWidget(durationLabel);

        bitRateLabel = new QLabel(layoutWidget);
        bitRateLabel->setObjectName("bitRateLabel");
        sizePolicy.setHeightForWidth(bitRateLabel->sizePolicy().hasHeightForWidth());
        bitRateLabel->setSizePolicy(sizePolicy);
        bitRateLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-height:40px;\n"
"	max-height:40px;\n"
"	min-width: 310px;\n"
"	max-width:310px;\n"
"}"));
        bitRateLabel->setWordWrap(true);

        verticalLayout->addWidget(bitRateLabel);

        frameRateLabel = new QLabel(layoutWidget);
        frameRateLabel->setObjectName("frameRateLabel");
        frameRateLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-height:40px;\n"
"	max-height:40px;\n"
"	min-width: 310px;\n"
"	max-width:310px;\n"
"}"));
        frameRateLabel->setWordWrap(true);

        verticalLayout->addWidget(frameRateLabel);

        nameLabel = new QLabel(layoutWidget);
        nameLabel->setObjectName("nameLabel");
        sizePolicy.setHeightForWidth(nameLabel->sizePolicy().hasHeightForWidth());
        nameLabel->setSizePolicy(sizePolicy);
        nameLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-height:40px;\n"
"	max-height:40px;\n"
"	min-width: 310px;\n"
"	max-width:310px;\n"
"}"));
        nameLabel->setWordWrap(true);

        verticalLayout->addWidget(nameLabel);

        picFmtLabel = new QLabel(layoutWidget);
        picFmtLabel->setObjectName("picFmtLabel");
        picFmtLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-height:40px;\n"
"	max-height:40px;\n"
"	min-width: 310px;\n"
"	max-width:310px;\n"
"}"));
        picFmtLabel->setWordWrap(true);

        verticalLayout->addWidget(picFmtLabel);

        aChannelsLabel = new QLabel(layoutWidget);
        aChannelsLabel->setObjectName("aChannelsLabel");
        sizePolicy.setHeightForWidth(aChannelsLabel->sizePolicy().hasHeightForWidth());
        aChannelsLabel->setSizePolicy(sizePolicy);
        aChannelsLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-height:40px;\n"
"	max-height:40px;\n"
"	min-width: 310px;\n"
"	max-width:310px;\n"
"}"));
        aChannelsLabel->setWordWrap(true);

        verticalLayout->addWidget(aChannelsLabel);

        aSampleRateLabel = new QLabel(layoutWidget);
        aSampleRateLabel->setObjectName("aSampleRateLabel");
        sizePolicy.setHeightForWidth(aSampleRateLabel->sizePolicy().hasHeightForWidth());
        aSampleRateLabel->setSizePolicy(sizePolicy);
        aSampleRateLabel->setStyleSheet(QString::fromUtf8("QLabel{\n"
"	min-height:40px;\n"
"	max-height:40px;\n"
"	min-width: 310px;\n"
"	max-width:310px;\n"
"}"));
        aSampleRateLabel->setWordWrap(true);

        verticalLayout->addWidget(aSampleRateLabel);


        retranslateUi(VideoInfoDialog);

        QMetaObject::connectSlotsByName(VideoInfoDialog);
    } // setupUi

    void retranslateUi(QDialog *VideoInfoDialog)
    {
        VideoInfoDialog->setWindowTitle(QCoreApplication::translate("VideoInfoDialog", "Dialog", nullptr));
        fileNameLabel->setText(QCoreApplication::translate("VideoInfoDialog", "TextLabel", nullptr));
        durationLabel->setText(QCoreApplication::translate("VideoInfoDialog", "TextLabel", nullptr));
        bitRateLabel->setText(QCoreApplication::translate("VideoInfoDialog", "TextLabel", nullptr));
        frameRateLabel->setText(QCoreApplication::translate("VideoInfoDialog", "TextLabel", nullptr));
        nameLabel->setText(QCoreApplication::translate("VideoInfoDialog", "TextLabel", nullptr));
        picFmtLabel->setText(QCoreApplication::translate("VideoInfoDialog", "TextLabel", nullptr));
        aChannelsLabel->setText(QCoreApplication::translate("VideoInfoDialog", "TextLabel", nullptr));
        aSampleRateLabel->setText(QCoreApplication::translate("VideoInfoDialog", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VideoInfoDialog: public Ui_VideoInfoDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VIDEOINFODIALOG_H
