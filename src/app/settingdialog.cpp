#include "settingdialog.h"
#include "ui_settingdialog.h"
#include <QFileDialog>
#include <QButtonGroup>
#include <QOverload>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include "configmanager.h"

settingDialog::settingDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::settingDialog)
{
    ui->setupUi(this);
    setWindowTitle("播放器设置");

    ConfigManager& cfg = ConfigManager::instance();

    // 备份原始值，用于取消时还原
    originalHardware_ = cfg.isHardware();
    originalSizeMode_ = cfg.getVideoSizeMode();
    originalBrightness_ = cfg.getBrightness();
    originalContrast_ = cfg.getContrast();
    originalSaturation_ = cfg.getSaturation();
    originalScreenshotPath_ = cfg.getScreenshotSavePath();
    originalModelPath_ = cfg.getModelPath();

    //初始化QRadioButton
    QButtonGroup *group1 = new QButtonGroup(this);
    group1->addButton(ui->softwareRadio);
    group1->addButton(ui->hardwareRadio);
    if (cfg.isHardware()) {
        ui->hardwareRadio->setChecked(true);
    } else {
        ui->softwareRadio->setChecked(true);
    }

    QButtonGroup *group2 = new QButtonGroup(this);
    group2->addButton(ui->defaultSize,0);
    group2->addButton(ui->expandSize,1);
    int sizeMode = cfg.getVideoSizeMode();
    if (sizeMode == 0) {
        ui->defaultSize->setChecked(true);
    } else {
        ui->expandSize->setChecked(true);
    }

    //初始化亮度调节QSlider
    ui->lightSlider->setRange(-100,100);
    ui->lightSlider->setValue(cfg.getBrightness());
    ui->lightLabel->setText(QString::number(cfg.getBrightness()));
    ui->contrastSlider->setRange(0,300);
    ui->contrastSlider->setValue(cfg.getContrast());
    ui->contrastLabel->setText(QString::number(cfg.getContrast()));
    ui->baoheSlider->setRange(0,300);
    ui->baoheSlider->setValue(cfg.getSaturation());
    ui->baoheLabel->setText(QString::number(cfg.getSaturation()));

    // 加载截图保存路径
    ui->saveFilePath->setText(cfg.getScreenshotSavePath());

    // 加载模型路径
    ui->modelPathLineEdit->setText(cfg.getModelPath());

    // AI 总结相关配置已迁移到 SummaryPanel 工具栏的 ⚙ 按钮
    // (见 SummarySettingsDialog),此处不再维护

    connect(group2, &QButtonGroup::buttonClicked, this, [=](QAbstractButton* button){
        int id = group2->id(button);
        on_SizeModeChanged(id);
    });
    connect(ui->decodeCombox,&QComboBox::currentTextChanged,this,[=](const QString &text){
        qDebug() << "当前解码器" << text;
        emit updateUserDecoder(text);
    });


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
    ui->lightLabel->setText(QString::number(value));
    emit brightnessValueChanged(value);
}


void settingDialog::on_contrastSlider_valueChanged(int value)
{
    ui->contrastLabel->setText(QString::number(value));
    emit contrastValueChanged(value);
}


void settingDialog::on_baoheSlider_valueChanged(int value)
{
    ui->baoheLabel->setText(QString::number(value));
    emit saturationValueChanged(value);
}

void settingDialog::on_uploadModelPathBtn_clicked()
{
    QString modelPath = QFileDialog::getOpenFileName(this,
                                                    "选择模型文件",
                                                    QDir::homePath(),
                                                    "模型文件(*.bin)");
    if(modelPath.isEmpty()) return;
    emit updateModelPath(modelPath);
    ui->modelPathLineEdit->setText(modelPath);
}

void settingDialog::on_confirmBtn_clicked()
{
    ConfigManager& cfg = ConfigManager::instance();
    cfg.setHardware(ui->hardwareRadio->isChecked());
    cfg.setVideoSizeMode(ui->defaultSize->isChecked() ? 0 : 1);
    cfg.setBrightness(ui->lightSlider->value());
    cfg.setContrast(ui->contrastSlider->value());
    cfg.setSaturation(ui->baoheSlider->value());
    cfg.setScreenshotSavePath(ui->saveFilePath->text().trimmed());
    QString modelPath = ui->modelPathLineEdit->text().trimmed();
    cfg.setModelPath(modelPath);
    emit updateModelPath(modelPath);

    // AI 总结相关配置由 SummarySettingsDialog 写入,此处不再处理
    cfg.save();
    accept();
}

void settingDialog::on_cancelBtn_clicked()
{
    ConfigManager& cfg = ConfigManager::instance();
    cfg.setHardware(originalHardware_);
    cfg.setVideoSizeMode(originalSizeMode_);
    cfg.setBrightness(originalBrightness_);
    cfg.setContrast(originalContrast_);
    cfg.setSaturation(originalSaturation_);
    cfg.setScreenshotSavePath(originalScreenshotPath_);
    cfg.setModelPath(originalModelPath_);

    reject();
}

void settingDialog::on_resetConfigBtn_clicked()
{
    ui->lightSlider->setValue(0);
    ui->lightLabel->setText("0");
    ui->contrastSlider->setValue(100);
    ui->contrastLabel->setText("100");
    ui->baoheSlider->setValue(100);
    ui->baoheLabel->setText("100");
}

