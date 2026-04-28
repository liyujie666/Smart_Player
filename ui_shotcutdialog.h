/********************************************************************************
** Form generated from reading UI file 'shotcutdialog.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SHOTCUTDIALOG_H
#define UI_SHOTCUTDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTableWidget>

QT_BEGIN_NAMESPACE

class Ui_ShotCutDialog
{
public:
    QTableWidget *tableWidget;

    void setupUi(QDialog *ShotCutDialog)
    {
        if (ShotCutDialog->objectName().isEmpty())
            ShotCutDialog->setObjectName("ShotCutDialog");
        ShotCutDialog->resize(265, 220);
        ShotCutDialog->setMinimumSize(QSize(265, 220));
        ShotCutDialog->setMaximumSize(QSize(265, 220));
        ShotCutDialog->setStyleSheet(QString::fromUtf8("/*QDialog {\n"
"    background: transparent;\n"
"}\n"
"*/"));
        tableWidget = new QTableWidget(ShotCutDialog);
        if (tableWidget->columnCount() < 2)
            tableWidget->setColumnCount(2);
        if (tableWidget->rowCount() < 7)
            tableWidget->setRowCount(7);
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/SmartPlayer-icon/space.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        QBrush brush(QColor(0, 0, 0, 255));
        brush.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush1(QColor(0, 0, 0, 250));
        brush1.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        __qtablewidgetitem->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem->setBackground(brush1);
        __qtablewidgetitem->setForeground(brush);
        __qtablewidgetitem->setIcon(icon);
        __qtablewidgetitem->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(0, 0, __qtablewidgetitem);
        QBrush brush2(QColor(0, 0, 0, 255));
        brush2.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush3(QColor(0, 0, 0, 250));
        brush3.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        __qtablewidgetitem1->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem1->setBackground(brush3);
        __qtablewidgetitem1->setForeground(brush2);
        __qtablewidgetitem1->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(0, 1, __qtablewidgetitem1);
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/SmartPlayer-icon/esc.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        QBrush brush4(QColor(0, 0, 0, 255));
        brush4.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush5(QColor(0, 0, 0, 250));
        brush5.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        __qtablewidgetitem2->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem2->setBackground(brush5);
        __qtablewidgetitem2->setForeground(brush4);
        __qtablewidgetitem2->setIcon(icon1);
        __qtablewidgetitem2->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(1, 0, __qtablewidgetitem2);
        QBrush brush6(QColor(0, 0, 0, 255));
        brush6.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush7(QColor(0, 0, 0, 250));
        brush7.setStyle(Qt::BrushStyle::NoBrush);
        QFont font;
        font.setKerning(true);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        __qtablewidgetitem3->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem3->setFont(font);
        __qtablewidgetitem3->setBackground(brush7);
        __qtablewidgetitem3->setForeground(brush6);
        __qtablewidgetitem3->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(1, 1, __qtablewidgetitem3);
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/SmartPlayer-icon/arrow_up.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        QBrush brush8(QColor(0, 0, 0, 255));
        brush8.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush9(QColor(0, 0, 0, 250));
        brush9.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        __qtablewidgetitem4->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem4->setBackground(brush9);
        __qtablewidgetitem4->setForeground(brush8);
        __qtablewidgetitem4->setIcon(icon2);
        __qtablewidgetitem4->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(2, 0, __qtablewidgetitem4);
        QBrush brush10(QColor(0, 0, 0, 255));
        brush10.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush11(QColor(0, 0, 0, 250));
        brush11.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem5 = new QTableWidgetItem();
        __qtablewidgetitem5->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem5->setBackground(brush11);
        __qtablewidgetitem5->setForeground(brush10);
        __qtablewidgetitem5->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(2, 1, __qtablewidgetitem5);
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/SmartPlayer-icon/arrow_down.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        QBrush brush12(QColor(0, 0, 0, 255));
        brush12.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush13(QColor(0, 0, 0, 250));
        brush13.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem6 = new QTableWidgetItem();
        __qtablewidgetitem6->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem6->setBackground(brush13);
        __qtablewidgetitem6->setForeground(brush12);
        __qtablewidgetitem6->setIcon(icon3);
        __qtablewidgetitem6->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(3, 0, __qtablewidgetitem6);
        QBrush brush14(QColor(0, 0, 0, 255));
        brush14.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush15(QColor(0, 0, 0, 250));
        brush15.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem7 = new QTableWidgetItem();
        __qtablewidgetitem7->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem7->setBackground(brush15);
        __qtablewidgetitem7->setForeground(brush14);
        __qtablewidgetitem7->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(3, 1, __qtablewidgetitem7);
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/SmartPlayer-icon/arrow_left.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        QBrush brush16(QColor(0, 0, 0, 255));
        brush16.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush17(QColor(0, 0, 0, 250));
        brush17.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem8 = new QTableWidgetItem();
        __qtablewidgetitem8->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem8->setBackground(brush17);
        __qtablewidgetitem8->setForeground(brush16);
        __qtablewidgetitem8->setIcon(icon4);
        __qtablewidgetitem8->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(4, 0, __qtablewidgetitem8);
        QBrush brush18(QColor(0, 0, 0, 255));
        brush18.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush19(QColor(0, 0, 0, 250));
        brush19.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem9 = new QTableWidgetItem();
        __qtablewidgetitem9->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem9->setBackground(brush19);
        __qtablewidgetitem9->setForeground(brush18);
        __qtablewidgetitem9->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(4, 1, __qtablewidgetitem9);
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/SmartPlayer-icon/arrow_right.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        QBrush brush20(QColor(0, 0, 0, 255));
        brush20.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush21(QColor(0, 0, 0, 250));
        brush21.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem10 = new QTableWidgetItem();
        __qtablewidgetitem10->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem10->setBackground(brush21);
        __qtablewidgetitem10->setForeground(brush20);
        __qtablewidgetitem10->setIcon(icon5);
        __qtablewidgetitem10->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(5, 0, __qtablewidgetitem10);
        QBrush brush22(QColor(0, 0, 0, 255));
        brush22.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush23(QColor(0, 0, 0, 250));
        brush23.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem11 = new QTableWidgetItem();
        __qtablewidgetitem11->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem11->setBackground(brush23);
        __qtablewidgetitem11->setForeground(brush22);
        __qtablewidgetitem11->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(5, 1, __qtablewidgetitem11);
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/SmartPlayer-icon/double_click.png"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        QBrush brush24(QColor(0, 0, 0, 255));
        brush24.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush25(QColor(0, 0, 0, 250));
        brush25.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem12 = new QTableWidgetItem();
        __qtablewidgetitem12->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem12->setBackground(brush25);
        __qtablewidgetitem12->setForeground(brush24);
        __qtablewidgetitem12->setIcon(icon6);
        __qtablewidgetitem12->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(6, 0, __qtablewidgetitem12);
        QBrush brush26(QColor(0, 0, 0, 255));
        brush26.setStyle(Qt::BrushStyle::NoBrush);
        QBrush brush27(QColor(0, 0, 0, 250));
        brush27.setStyle(Qt::BrushStyle::NoBrush);
        QTableWidgetItem *__qtablewidgetitem13 = new QTableWidgetItem();
        __qtablewidgetitem13->setTextAlignment(Qt::AlignCenter);
        __qtablewidgetitem13->setBackground(brush27);
        __qtablewidgetitem13->setForeground(brush26);
        __qtablewidgetitem13->setFlags(Qt::NoItemFlags);
        tableWidget->setItem(6, 1, __qtablewidgetitem13);
        tableWidget->setObjectName("tableWidget");
        tableWidget->setGeometry(QRect(0, 0, 265, 220));
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(tableWidget->sizePolicy().hasHeightForWidth());
        tableWidget->setSizePolicy(sizePolicy);
        tableWidget->setMinimumSize(QSize(265, 220));
        tableWidget->setMaximumSize(QSize(265, 220));
        tableWidget->setStyleSheet(QString::fromUtf8("QTableWidget { \n"
"	background:  rgba(0, 0, 0, 160);\n"
"    border: none;\n"
"	color:white;\n"
" }\n"
"\n"
"QTableWidget::item {\n"
"    background:  rgba(0, 0, 0, 160);\n"
"    border: none; /* \345\217\257\351\200\211\357\274\232\347\247\273\351\231\244\345\215\225\345\205\203\346\240\274\350\276\271\346\241\206 */\n"
"}\n"
"\n"
"QTableWidget::item:selected {\n"
"    background: rgba(0, 0, 0, 160); /* \345\215\212\351\200\217\346\230\216\351\273\221\350\211\262 */\n"
"    color: white; /* \351\200\211\344\270\255\346\226\207\346\234\254\351\242\234\350\211\262 */\n"
"}"));
        tableWidget->setLineWidth(1);
        tableWidget->setMidLineWidth(0);
        tableWidget->setShowGrid(true);
        tableWidget->setGridStyle(Qt::PenStyle::SolidLine);
        tableWidget->setSortingEnabled(false);
        tableWidget->setRowCount(7);
        tableWidget->setColumnCount(2);
        tableWidget->horizontalHeader()->setVisible(false);
        tableWidget->horizontalHeader()->setDefaultSectionSize(130);
        tableWidget->horizontalHeader()->setHighlightSections(false);
        tableWidget->verticalHeader()->setVisible(false);
        tableWidget->verticalHeader()->setHighlightSections(false);

        retranslateUi(ShotCutDialog);

        QMetaObject::connectSlotsByName(ShotCutDialog);
    } // setupUi

    void retranslateUi(QDialog *ShotCutDialog)
    {
        ShotCutDialog->setWindowTitle(QCoreApplication::translate("ShotCutDialog", "Dialog", nullptr));

        const bool __sortingEnabled = tableWidget->isSortingEnabled();
        tableWidget->setSortingEnabled(false);
        QTableWidgetItem *___qtablewidgetitem = tableWidget->item(0, 0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("ShotCutDialog", "Space", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = tableWidget->item(0, 1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("ShotCutDialog", "\346\222\255\346\224\276/\346\232\202\345\201\234", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = tableWidget->item(1, 0);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("ShotCutDialog", "Esc", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = tableWidget->item(1, 1);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("ShotCutDialog", "\351\200\200\345\207\272\345\205\250\345\261\217", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = tableWidget->item(2, 0);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("ShotCutDialog", "Up", nullptr));
        QTableWidgetItem *___qtablewidgetitem5 = tableWidget->item(2, 1);
        ___qtablewidgetitem5->setText(QCoreApplication::translate("ShotCutDialog", "\351\237\263\351\207\217\345\242\236\345\212\24010%", nullptr));
        QTableWidgetItem *___qtablewidgetitem6 = tableWidget->item(3, 0);
        ___qtablewidgetitem6->setText(QCoreApplication::translate("ShotCutDialog", "Down", nullptr));
        QTableWidgetItem *___qtablewidgetitem7 = tableWidget->item(3, 1);
        ___qtablewidgetitem7->setText(QCoreApplication::translate("ShotCutDialog", "\351\237\263\351\207\217\345\207\217\345\260\22110%", nullptr));
        QTableWidgetItem *___qtablewidgetitem8 = tableWidget->item(4, 0);
        ___qtablewidgetitem8->setText(QCoreApplication::translate("ShotCutDialog", "Left", nullptr));
        QTableWidgetItem *___qtablewidgetitem9 = tableWidget->item(4, 1);
        ___qtablewidgetitem9->setText(QCoreApplication::translate("ShotCutDialog", "\345\220\216\351\200\20010s", nullptr));
        QTableWidgetItem *___qtablewidgetitem10 = tableWidget->item(5, 0);
        ___qtablewidgetitem10->setText(QCoreApplication::translate("ShotCutDialog", "Right", nullptr));
        QTableWidgetItem *___qtablewidgetitem11 = tableWidget->item(5, 1);
        ___qtablewidgetitem11->setText(QCoreApplication::translate("ShotCutDialog", "\345\277\253\350\277\23310s \351\225\277\346\214\211\345\200\215\351\200\237\346\222\255\346\224\276", nullptr));
        QTableWidgetItem *___qtablewidgetitem12 = tableWidget->item(6, 0);
        ___qtablewidgetitem12->setText(QCoreApplication::translate("ShotCutDialog", "Double Click", nullptr));
        QTableWidgetItem *___qtablewidgetitem13 = tableWidget->item(6, 1);
        ___qtablewidgetitem13->setText(QCoreApplication::translate("ShotCutDialog", "\345\205\250\345\261\217/\351\200\200\345\207\272\345\205\250\345\261\217", nullptr));
        tableWidget->setSortingEnabled(__sortingEnabled);

    } // retranslateUi

};

namespace Ui {
    class ShotCutDialog: public Ui_ShotCutDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SHOTCUTDIALOG_H
