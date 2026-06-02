#include "settingdialog.h"
#include "ui_settingdialog.h"
#include <QFileDialog>
#include <QButtonGroup>
#include <QOverload>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
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

    // 创建 AI 视频总结配置区域
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(ui->verticalLayout);
    if (mainLayout) {
        // 分隔线
        QFrame* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("QFrame { color: #555; }");
        mainLayout->addWidget(line);

        // AI 总结标题
        QLabel* summaryTitle = new QLabel(QStringLiteral(u"AI 视频总结:"), this);
    summaryTitle->setStyleSheet(QStringLiteral(
        u"QLabel { color: white; font-size: 13px; font-weight: bold; }"));
        mainLayout->addWidget(summaryTitle);

        // API Key 行
        QHBoxLayout* apiKeyRow = new QHBoxLayout();
        QLabel* apiKeyLabel = new QLabel(QString::fromLatin1("API Key:"), this);
        apiKeyLabel->setStyleSheet(QString::fromLatin1("QLabel { color: white; font-size: 13px; }"));
        apiKeyLabel->setFixedWidth(80);
        m_summaryApiKeyLine = new QLineEdit(this);
        m_summaryApiKeyLine->setStyleSheet(QString::fromLatin1(
            "QLineEdit { color: white; background-color: #3c3c3c; font-size: 12px; }"));
        m_summaryApiKeyLine->setPlaceholderText(QString::fromLatin1("sk-xxxxxxxxxxxxxxxx"));
        m_summaryApiKeyLine->setText(cfg.getSummaryApiKey());
        m_summaryApiKeyLine->setEchoMode(QLineEdit::Password);
        apiKeyRow->addWidget(apiKeyLabel);
        apiKeyRow->addWidget(m_summaryApiKeyLine);
        mainLayout->addLayout(apiKeyRow);

        // 端点 URL 行
        QHBoxLayout* endpointRow = new QHBoxLayout();
        QLabel* endpointLabel = new QLabel(QStringLiteral(u"端点:"), this);
        endpointLabel->setStyleSheet(QString::fromLatin1("QLabel { color: white; font-size: 13px; }"));
        endpointLabel->setFixedWidth(80);
        m_summaryEndpointLine = new QLineEdit(this);
        m_summaryEndpointLine->setStyleSheet(QString::fromLatin1(
            "QLineEdit { color: white; background-color: #3c3c3c; font-size: 12px; }"));
        m_summaryEndpointLine->setText(cfg.getSummaryModelEndpoint());
        endpointRow->addWidget(endpointLabel);
        endpointRow->addWidget(m_summaryEndpointLine);
        mainLayout->addLayout(endpointRow);

        // 模型选择行
        QHBoxLayout* modelRow = new QHBoxLayout();
        QLabel* modelLabel = new QLabel(QStringLiteral(u"VLM 模型:"), this);
        modelLabel->setStyleSheet(QString::fromLatin1("QLabel { color: white; font-size: 13px; }"));
        modelLabel->setFixedWidth(80);
        m_summaryModelCombo = new QComboBox(this);
        m_summaryModelCombo->setStyleSheet(QString::fromLatin1(
            "QComboBox { color: white; background-color: #3c3c3c; font-size: 12px; }"));
        m_summaryModelCombo->addItem(QStringLiteral(u"qwen-vl-plus (均衡)"), "qwen-vl-plus");
        m_summaryModelCombo->addItem(QStringLiteral(u"qwen-vl-max (最强)"), "qwen-vl-max");
        m_summaryModelCombo->addItem(QStringLiteral(u"qwen-vl-flash (快速)"), "qwen-vl-flash");
        m_summaryModelCombo->setCurrentText(cfg.getSummaryModel().isEmpty()
            ? QStringLiteral(u"qwen-vl-plus (均衡)")
            : cfg.getSummaryModel());
        modelRow->addWidget(modelLabel);
        modelRow->addWidget(m_summaryModelCombo);
        modelRow->addStretch();
        mainLayout->addLayout(modelRow);

        // 分段时长行
        QHBoxLayout* segmentRow = new QHBoxLayout();
        QLabel* segLabel = new QLabel(QStringLiteral(u"分段时长:"), this);
        segLabel->setStyleSheet(QString::fromLatin1("QLabel { color: white; font-size: 13px; }"));
        segLabel->setFixedWidth(80);
        m_summarySegmentDurationSpin = new QSpinBox(this);
        m_summarySegmentDurationSpin->setStyleSheet(QString::fromLatin1(
            "QSpinBox { color: white; background-color: #3c3c3c; font-size: 12px; }"));
        m_summarySegmentDurationSpin->setRange(1000, 60000);
        m_summarySegmentDurationSpin->setSingleStep(1000);
        m_summarySegmentDurationSpin->setValue(cfg.getSummarySegmentDuration());
        m_summarySegmentDurationLabel = new QLabel(QString::fromLatin1("ms"), this);
        m_summarySegmentDurationLabel->setStyleSheet(QString::fromLatin1(
            "QLabel { color: #aaa; font-size: 12px; }"));
        segmentRow->addWidget(segLabel);
        segmentRow->addWidget(m_summarySegmentDurationSpin);
        segmentRow->addWidget(m_summarySegmentDurationLabel);
        segmentRow->addStretch();
        mainLayout->addLayout(segmentRow);
    }

    // 扩大对话框高度以容纳新控件
    setMinimumHeight(650);
    resize(width(), 650);

    connect(m_summaryApiKeyLine, &QLineEdit::textChanged,
            this, &settingDialog::on_summaryApiKeyChanged);
    connect(m_summaryEndpointLine, &QLineEdit::textChanged,
            this, &settingDialog::on_summaryEndpointChanged);
    connect(m_summarySegmentDurationSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &settingDialog::on_summarySegmentDurationChanged);
    connect(m_summaryModelCombo, &QComboBox::currentTextChanged,
            this, &settingDialog::on_summaryModelChanged);

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

    // 保存 AI 视频总结配置
    if (m_summaryApiKeyLine) cfg.setSummaryApiKey(m_summaryApiKeyLine->text().trimmed());
    if (m_summaryEndpointLine) cfg.setSummaryModelEndpoint(m_summaryEndpointLine->text().trimmed());
    if (m_summarySegmentDurationSpin) cfg.setSummarySegmentDuration(m_summarySegmentDurationSpin->value());
    if (m_summaryModelCombo) cfg.setSummaryModel(m_summaryModelCombo->currentData().toString());
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

void settingDialog::on_summaryApiKeyChanged(const QString& text) {
    ConfigManager::instance().setSummaryApiKey(text);
}

void settingDialog::on_summaryEndpointChanged(const QString& text) {
    ConfigManager::instance().setSummaryModelEndpoint(text);
}

void settingDialog::on_summarySegmentDurationChanged(int value) {
    ConfigManager::instance().setSummarySegmentDuration(value);
}

void settingDialog::on_summaryModelChanged(const QString& text) {
    Q_UNUSED(text);
    ConfigManager::instance().setSummaryModel(m_summaryModelCombo->currentData().toString());
}


