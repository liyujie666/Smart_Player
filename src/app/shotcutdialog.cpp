#include "shotcutdialog.h"
#include "ui_shotcutdialog.h"

ShotCutDialog::ShotCutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ShotCutDialog)
{
    ui->setupUi(this);
    setWindowTitle("快捷键说明");
}

ShotCutDialog::~ShotCutDialog()
{
    delete ui;
}
