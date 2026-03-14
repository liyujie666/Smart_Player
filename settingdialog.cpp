#include "settingdialog.h"
#include "ui_settingdialog.h"
#include <QFileDialog>
#include <QButtonGroup>
#include <QOverload>
#include <QDebug>

settingDialog::settingDialog(VideoWidget *videoWidget,QWidget *parent)
    : QDialog(parent)
    , videoWidget_(videoWidget)
    , ui(new Ui::settingDialog)
{
    ui->setupUi(this);
    setWindowTitle("播放器设置");
    //初始化QRadioButton
    QButtonGroup *group1 = new QButtonGroup(this);
    group1->addButton(ui->softwareRadio);
    group1->addButton(ui->hardwareRadio);
    ui->softwareRadio->setChecked(true);

    QButtonGroup *group2 = new QButtonGroup(this);
    group2->addButton(ui->defaultSize);
    group2->addButton(ui->stretchSize);
    group2->addButton(ui->expandSize);
    group2->addButton(ui->cutSize);
    ui->defaultSize->setChecked(true);

    //初始化亮度调节QSlider
    ui->lightSlider->setRange(0,100);
    ui->lightSlider->setValue(50);

    connect(group2, &QButtonGroup::buttonClicked, this, [=](QAbstractButton* button){
        int id = group2->id(button);
        on_SizeModeChanged(id);
    });
    connect(ui->decodeCombox,&QComboBox::currentTextChanged,this,[=](const QString &text){
        qDebug() << "当前解码器" << text;
        emit updateUserDecoder(text);
    });
    connect(ui->lightSlider,&QSlider::valueChanged,videoWidget_,&VideoWidget::setBrightness);

}

settingDialog::~settingDialog()
{
    delete ui;
}

bool settingDialog::getIsHardWare() const
{
    return ui->hardwareRadio->isChecked();
}

void settingDialog::on_hardwareRadio_toggled(bool checked)
{
    if(checked && ui->hardwareRadio->isChecked()){
        emit startHardWareAccep(true);
    }
}


void settingDialog::on_softwareRadio_toggled(bool checked)
{
    if(checked && ui->softwareRadio->isChecked()){
        emit startSoftWareAccep(false);
    }
}


void settingDialog::on_selectPathBtn_clicked()
{
    QString savePath = QFileDialog::getExistingDirectory(this,"选择保存路径",QDir::homePath());
    if(!savePath.isEmpty()){
        qDebug() << savePath;
        ui->saveFilePath->setText(savePath);
        emit updateSaveFilePath(savePath);
    }
}

void settingDialog::on_SizeModeChanged(int mode)
{
    emit updateVideoSizeMode(mode);
}


void settingDialog::on_lightSlider_valueChanged(int value)
{
    ui->lightLabel->setText(QString("%1").arg(value));
}

