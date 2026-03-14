#ifndef SHOTCUTDIALOG_H
#define SHOTCUTDIALOG_H

#include <QDialog>

namespace Ui {
class ShotCutDialog;
}

class ShotCutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShotCutDialog(QWidget *parent = nullptr);
    ~ShotCutDialog();

private:
    Ui::ShotCutDialog *ui;
};

#endif // SHOTCUTDIALOG_H
