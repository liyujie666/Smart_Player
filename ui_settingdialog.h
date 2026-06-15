/********************************************************************************
** Form generated from reading UI file 'settingdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGDIALOG_H
#define UI_SETTINGDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_settingDialog
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QRadioButton *softwareRadio;
    QRadioButton *hardwareRadio;
    QSpacerItem *verticalSpacer_4;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_2;
    QComboBox *decodeCombox;
    QSpacerItem *horizontalSpacer;
    QSpacerItem *verticalSpacer_3;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_3;
    QSlider *lightSlider;
    QLabel *lightLabel;
    QSpacerItem *verticalSpacer_2;
    QHBoxLayout *horizontalLayout_7;
    QLabel *contrastLabel1;
    QSlider *contrastSlider;
    QLabel *contrastLabel;
    QSpacerItem *verticalSpacer_5;
    QHBoxLayout *horizontalLayout_8;
    QLabel *baoheLabel1;
    QSlider *baoheSlider;
    QLabel *baoheLabel;
    QHBoxLayout *horizontalLayout_10;
    QSpacerItem *horizontalSpacer_3;
    QPushButton *resetConfigBtn;
    QHBoxLayout *horizontalLayout_4;
    QLabel *label_5;
    QRadioButton *defaultSize;
    QRadioButton *expandSize;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayout_5;
    QLabel *label_6;
    QLineEdit *saveFilePath;
    QPushButton *selectPathBtn;
    QSpacerItem *verticalSpacer_7;
    QHBoxLayout *horizontalLayout_6;
    QLabel *label_7;
    QLineEdit *modelPathLineEdit;
    QPushButton *uploadModelPathBtn;
    QSpacerItem *verticalSpacer_8;
    QHBoxLayout *horizontalLayout_9;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *cancelBtn;
    QPushButton *confirmBtn;

    void setupUi(QDialog *settingDialog)
    {
        if (settingDialog->objectName().isEmpty())
            settingDialog->setObjectName("settingDialog");
        settingDialog->resize(350, 500);
        settingDialog->setMinimumSize(QSize(350, 500));
        settingDialog->setMaximumSize(QSize(350, 500));
        settingDialog->setStyleSheet(QString::fromUtf8("QRadioButton{\n"
"	font-size:13px;\n"
"}\n"
"QLabel { \n"
"font-size:13px;\n"
" }\n"
"QPushButton{\n"
"	max-height:17px;\n"
"	max-width:50px;\n"
"	min-height:17px;\n"
"	min-width:50px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}\n"
"QComboBox{\n"
"	font-size:13px;\n"
"	min-width:30px;\n"
"}\n"
"QComboBox QAbstractItemView {\n"
"    color: white;\n"
"}\n"
"QLineEdit{\n"
"font-size:12px;\n"
"}"));
        verticalLayout = new QVBoxLayout(settingDialog);
        verticalLayout->setObjectName("verticalLayout");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(settingDialog);
        label->setObjectName("label");
        label->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout->addWidget(label);

        softwareRadio = new QRadioButton(settingDialog);
        softwareRadio->setObjectName("softwareRadio");
        softwareRadio->setStyleSheet(QString::fromUtf8("QRadioButton{\n"
"	font-size:13px;\n"
"}"));
        softwareRadio->setChecked(true);

        horizontalLayout->addWidget(softwareRadio);

        hardwareRadio = new QRadioButton(settingDialog);
        hardwareRadio->setObjectName("hardwareRadio");
        hardwareRadio->setStyleSheet(QString::fromUtf8("QRadioButton{\n"
"	font-size:13px;\n"
"}"));

        horizontalLayout->addWidget(hardwareRadio);


        verticalLayout->addLayout(horizontalLayout);

        verticalSpacer_4 = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer_4);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        label_2 = new QLabel(settingDialog);
        label_2->setObjectName("label_2");
        label_2->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_2->addWidget(label_2);

        decodeCombox = new QComboBox(settingDialog);
        decodeCombox->addItem(QString());
        decodeCombox->addItem(QString());
        decodeCombox->addItem(QString());
        decodeCombox->addItem(QString());
        decodeCombox->addItem(QString());
        decodeCombox->addItem(QString());
        decodeCombox->addItem(QString());
        decodeCombox->addItem(QString());
        decodeCombox->setObjectName("decodeCombox");
        decodeCombox->setStyleSheet(QString::fromUtf8("QComboBox{\n"
"	font-size:13px;\n"
"	min-width:30px;\n"
"}\n"
"QComboBox QAbstractItemView::item{\n"
"color:black;\n"
"}"));

        horizontalLayout_2->addWidget(decodeCombox);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);


        verticalLayout->addLayout(horizontalLayout_2);

        verticalSpacer_3 = new QSpacerItem(20, 18, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer_3);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        label_3 = new QLabel(settingDialog);
        label_3->setObjectName("label_3");
        label_3->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_3->addWidget(label_3);

        lightSlider = new QSlider(settingDialog);
        lightSlider->setObjectName("lightSlider");
        lightSlider->setStyleSheet(QString::fromUtf8("QSlider::groove:horizontal {\n"
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
        lightSlider->setValue(50);
        lightSlider->setOrientation(Qt::Orientation::Horizontal);

        horizontalLayout_3->addWidget(lightSlider);

        lightLabel = new QLabel(settingDialog);
        lightLabel->setObjectName("lightLabel");
        lightLabel->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_3->addWidget(lightLabel);


        verticalLayout->addLayout(horizontalLayout_3);

        verticalSpacer_2 = new QSpacerItem(20, 21, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);

        horizontalLayout_7 = new QHBoxLayout();
        horizontalLayout_7->setObjectName("horizontalLayout_7");
        contrastLabel1 = new QLabel(settingDialog);
        contrastLabel1->setObjectName("contrastLabel1");
        contrastLabel1->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_7->addWidget(contrastLabel1);

        contrastSlider = new QSlider(settingDialog);
        contrastSlider->setObjectName("contrastSlider");
        contrastSlider->setStyleSheet(QString::fromUtf8("QSlider::groove:horizontal {\n"
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
        contrastSlider->setValue(50);
        contrastSlider->setOrientation(Qt::Orientation::Horizontal);

        horizontalLayout_7->addWidget(contrastSlider);

        contrastLabel = new QLabel(settingDialog);
        contrastLabel->setObjectName("contrastLabel");
        contrastLabel->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_7->addWidget(contrastLabel);


        verticalLayout->addLayout(horizontalLayout_7);

        verticalSpacer_5 = new QSpacerItem(20, 20, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer_5);

        horizontalLayout_8 = new QHBoxLayout();
        horizontalLayout_8->setObjectName("horizontalLayout_8");
        baoheLabel1 = new QLabel(settingDialog);
        baoheLabel1->setObjectName("baoheLabel1");
        baoheLabel1->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_8->addWidget(baoheLabel1);

        baoheSlider = new QSlider(settingDialog);
        baoheSlider->setObjectName("baoheSlider");
        baoheSlider->setStyleSheet(QString::fromUtf8("QSlider::groove:horizontal {\n"
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
        baoheSlider->setValue(50);
        baoheSlider->setOrientation(Qt::Orientation::Horizontal);

        horizontalLayout_8->addWidget(baoheSlider);

        baoheLabel = new QLabel(settingDialog);
        baoheLabel->setObjectName("baoheLabel");
        baoheLabel->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_8->addWidget(baoheLabel);


        verticalLayout->addLayout(horizontalLayout_8);

        horizontalLayout_10 = new QHBoxLayout();
        horizontalLayout_10->setSpacing(0);
        horizontalLayout_10->setObjectName("horizontalLayout_10");
        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_10->addItem(horizontalSpacer_3);

        resetConfigBtn = new QPushButton(settingDialog);
        resetConfigBtn->setObjectName("resetConfigBtn");
        resetConfigBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	max-height:17px;\n"
"	max-width:50px;\n"
"	min-height:17px;\n"
"	min-width:50px;\n"
"	border:none;\n"
"background-color: transparent;\n"
"border-radius:5px;\n"
"}"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/SmartPlayer-icon/reset.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        resetConfigBtn->setIcon(icon);
        resetConfigBtn->setIconSize(QSize(17, 17));

        horizontalLayout_10->addWidget(resetConfigBtn);


        verticalLayout->addLayout(horizontalLayout_10);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        label_5 = new QLabel(settingDialog);
        label_5->setObjectName("label_5");
        label_5->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_4->addWidget(label_5);

        defaultSize = new QRadioButton(settingDialog);
        defaultSize->setObjectName("defaultSize");
        defaultSize->setStyleSheet(QString::fromUtf8("QRadioButton\n"
"{\n"
"font-size:13px;\n"
"}\n"
"\n"
""));
        defaultSize->setChecked(true);

        horizontalLayout_4->addWidget(defaultSize);

        expandSize = new QRadioButton(settingDialog);
        expandSize->setObjectName("expandSize");
        expandSize->setStyleSheet(QString::fromUtf8("QRadioButton\n"
"{\n"
"font-size:13px;\n"
"}\n"
""));

        horizontalLayout_4->addWidget(expandSize);


        verticalLayout->addLayout(horizontalLayout_4);

        verticalSpacer = new QSpacerItem(20, 28, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName("horizontalLayout_5");
        label_6 = new QLabel(settingDialog);
        label_6->setObjectName("label_6");
        label_6->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_5->addWidget(label_6);

        saveFilePath = new QLineEdit(settingDialog);
        saveFilePath->setObjectName("saveFilePath");
        saveFilePath->setMinimumSize(QSize(200, 0));
        saveFilePath->setMaximumSize(QSize(400, 16777215));
        saveFilePath->setStyleSheet(QString::fromUtf8("QLineEdit{\n"
"font-size:12px;\n"
"}"));

        horizontalLayout_5->addWidget(saveFilePath);

        selectPathBtn = new QPushButton(settingDialog);
        selectPathBtn->setObjectName("selectPathBtn");
        selectPathBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	font-size:13px;\n"
"	max-width:45px;\n"
"	min-width:45px;\n"
"	border-radius:5px;\n"
"	border:white;\n"
"}"));

        horizontalLayout_5->addWidget(selectPathBtn);


        verticalLayout->addLayout(horizontalLayout_5);

        verticalSpacer_7 = new QSpacerItem(20, 17, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer_7);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName("horizontalLayout_6");
        label_7 = new QLabel(settingDialog);
        label_7->setObjectName("label_7");
        label_7->setStyleSheet(QString::fromUtf8("QLabel { \n"
"font-size:13px;\n"
" }"));

        horizontalLayout_6->addWidget(label_7);

        modelPathLineEdit = new QLineEdit(settingDialog);
        modelPathLineEdit->setObjectName("modelPathLineEdit");
        modelPathLineEdit->setMinimumSize(QSize(200, 0));
        modelPathLineEdit->setMaximumSize(QSize(400, 16777215));
        modelPathLineEdit->setStyleSheet(QString::fromUtf8("QLineEdit{\n"
"font-size:12px;\n"
"}"));

        horizontalLayout_6->addWidget(modelPathLineEdit);

        uploadModelPathBtn = new QPushButton(settingDialog);
        uploadModelPathBtn->setObjectName("uploadModelPathBtn");
        uploadModelPathBtn->setStyleSheet(QString::fromUtf8("QPushButton{\n"
"	font-size:13px;\n"
"	max-width:45px;\n"
"	min-width:45px;\n"
"	border-radius:5px;\n"
"	border:white;\n"
"}"));

        horizontalLayout_6->addWidget(uploadModelPathBtn);


        verticalLayout->addLayout(horizontalLayout_6);

        verticalSpacer_8 = new QSpacerItem(20, 18, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer_8);

        horizontalLayout_9 = new QHBoxLayout();
        horizontalLayout_9->setObjectName("horizontalLayout_9");
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_9->addItem(horizontalSpacer_2);

        cancelBtn = new QPushButton(settingDialog);
        cancelBtn->setObjectName("cancelBtn");
        cancelBtn->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"    background-color: #f0f0f0;\n"
"    color: #333333;\n"
"    border: none;\n"
"    border-radius: 4px;\n"
"    padding: 4px 16px; \n"
"    font-size: 14px;\n"
"}\n"
"QPushButton:hover {\n"
"    background-color: #ffffff;\n"
"}\n"
"QPushButton:pressed {\n"
"    background-color: #dcdcdc;\n"
"}\n"
""));

        horizontalLayout_9->addWidget(cancelBtn);

        confirmBtn = new QPushButton(settingDialog);
        confirmBtn->setObjectName("confirmBtn");
        confirmBtn->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"    background-color: #e8589e;\n"
"    color: #ffffff;\n"
"    border: none;\n"
"    border-radius: 6px;\n"
"    padding: 4px 16px;\n"
"    font-size: 14px;\n"
"}\n"
"QPushButton:hover {\n"
"    background-color: #f06cae;\n"
"}\n"
"QPushButton:pressed {\n"
"    background-color: #d14a8a;\n"
"}\n"
""));

        horizontalLayout_9->addWidget(confirmBtn);


        verticalLayout->addLayout(horizontalLayout_9);


        retranslateUi(settingDialog);

        QMetaObject::connectSlotsByName(settingDialog);
    } // setupUi

    void retranslateUi(QDialog *settingDialog)
    {
        settingDialog->setWindowTitle(QCoreApplication::translate("settingDialog", "Dialog", nullptr));
        label->setText(QCoreApplication::translate("settingDialog", "\345\212\240\351\200\237\346\226\271\345\274\217\357\274\232", nullptr));
        softwareRadio->setText(QCoreApplication::translate("settingDialog", "\350\275\257\344\273\266\345\212\240\351\200\237", nullptr));
        hardwareRadio->setText(QCoreApplication::translate("settingDialog", "\347\241\254\344\273\266\345\212\240\351\200\237", nullptr));
        label_2->setText(QCoreApplication::translate("settingDialog", "\346\214\207\345\256\232\350\247\243\347\240\201\346\240\274\345\274\217\357\274\232", nullptr));
        decodeCombox->setItemText(0, QCoreApplication::translate("settingDialog", "\351\273\230\350\256\244(\346\216\250\350\215\220)", nullptr));
        decodeCombox->setItemText(1, QCoreApplication::translate("settingDialog", "h264", nullptr));
        decodeCombox->setItemText(2, QCoreApplication::translate("settingDialog", "h264_cuvid", nullptr));
        decodeCombox->setItemText(3, QCoreApplication::translate("settingDialog", "hevc", nullptr));
        decodeCombox->setItemText(4, QCoreApplication::translate("settingDialog", "hevc_cuvid", nullptr));
        decodeCombox->setItemText(5, QCoreApplication::translate("settingDialog", "mpeg4", nullptr));
        decodeCombox->setItemText(6, QCoreApplication::translate("settingDialog", "vp9", nullptr));
        decodeCombox->setItemText(7, QCoreApplication::translate("settingDialog", "av1", nullptr));

        label_3->setText(QCoreApplication::translate("settingDialog", "  \344\272\256\345\272\246\357\274\232", nullptr));
        lightLabel->setText(QCoreApplication::translate("settingDialog", "50", nullptr));
        contrastLabel1->setText(QCoreApplication::translate("settingDialog", "\345\257\271\346\257\224\345\272\246\357\274\232", nullptr));
        contrastLabel->setText(QCoreApplication::translate("settingDialog", "50", nullptr));
        baoheLabel1->setText(QCoreApplication::translate("settingDialog", "\351\245\261\345\222\214\345\272\246\357\274\232", nullptr));
        baoheLabel->setText(QCoreApplication::translate("settingDialog", "50", nullptr));
        resetConfigBtn->setText(QCoreApplication::translate("settingDialog", "\351\207\215\347\275\256", nullptr));
        label_5->setText(QCoreApplication::translate("settingDialog", "\347\224\273\351\235\242\345\260\272\345\257\270\357\274\232", nullptr));
        defaultSize->setText(QCoreApplication::translate("settingDialog", "\351\273\230\350\256\244", nullptr));
        expandSize->setText(QCoreApplication::translate("settingDialog", "\347\274\251\346\224\276\345\241\253\345\205\205", nullptr));
        label_6->setText(QCoreApplication::translate("settingDialog", "\344\277\235\345\255\230\350\267\257\345\276\204\357\274\232", nullptr));
        saveFilePath->setText(QString());
        saveFilePath->setPlaceholderText(QString());
        selectPathBtn->setText(QCoreApplication::translate("settingDialog", "\350\207\252\345\256\232\344\271\211", nullptr));
        label_7->setText(QCoreApplication::translate("settingDialog", "\346\250\241\345\236\213\350\267\257\345\276\204\357\274\232", nullptr));
        modelPathLineEdit->setText(QString());
        modelPathLineEdit->setPlaceholderText(QString());
        uploadModelPathBtn->setText(QCoreApplication::translate("settingDialog", "\344\270\212\344\274\240", nullptr));
        cancelBtn->setText(QCoreApplication::translate("settingDialog", "\345\217\226\346\266\210", nullptr));
        confirmBtn->setText(QCoreApplication::translate("settingDialog", "\347\241\256\345\256\232", nullptr));
    } // retranslateUi

};

namespace Ui {
    class settingDialog: public Ui_settingDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGDIALOG_H
