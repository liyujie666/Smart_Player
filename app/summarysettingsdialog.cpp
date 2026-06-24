#include "summarysettingsdialog.h"
#include "configmanager.h"
#include "videosummarymanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QIcon>
#include <QFont>

SummarySettingsDialog::SummarySettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral(u"AI \u603b\u7ed3\u8bbe\u7f6e"));
    setMinimumWidth(420);
    setModal(true);

    buildUI();
    loadFromConfig();
    updateCacheSizeLabel();
}

void SummarySettingsDialog::buildUI() {
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // ===== API Key =====
    QFormLayout* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    QHBoxLayout* apiKeyRow = new QHBoxLayout();
    m_apiKeyLine = new QLineEdit(this);
    m_apiKeyLine->setEchoMode(QLineEdit::Password);
    m_apiKeyLine->setPlaceholderText(QStringLiteral(u"sk-xxxxxxxxxxxxxxxx"));
    m_apiKeyShowBtn = new QToolButton(this);
    m_apiKeyShowBtn->setText(QStringLiteral(u"\u{1F441}"));
    m_apiKeyShowBtn->setCheckable(true);
    m_apiKeyShowBtn->setToolTip(QStringLiteral(u"\u663e\u793a/\u9690\u85cf API Key"));
    m_apiKeyShowBtn->setFixedWidth(32);
    apiKeyRow->addWidget(m_apiKeyLine, 1);
    apiKeyRow->addWidget(m_apiKeyShowBtn);
    form->addRow(QStringLiteral(u"API Key:"), apiKeyRow);

    // ===== 端点 =====
    m_endpointLine = new QLineEdit(this);
    m_endpointLine->setPlaceholderText(QStringLiteral(u"https://dashscope.aliyuncs.com/compatible-mode/v1"));
    form->addRow(QStringLiteral(u"\u7aef\u70b9:"), m_endpointLine);

    // ===== VLM 模型 =====
    m_modelCombo = new QComboBox(this);
    m_modelCombo->addItem(QStringLiteral(u"qwen-vl-plus (\u5747\u8861)"), "qwen-vl-plus");
    m_modelCombo->addItem(QStringLiteral(u"qwen-vl-max (\u6700\u5f3a)"), "qwen-vl-max");
    m_modelCombo->addItem(QStringLiteral(u"qwen-vl-flash (\u5feb\u901f)"), "qwen-vl-flash");
    form->addRow(QStringLiteral(u"VLM \u6a21\u578b:"), m_modelCombo);

    // ===== 分段时长 =====
    QHBoxLayout* segRow = new QHBoxLayout();
    m_segmentDurationSpin = new QSpinBox(this);
    m_segmentDurationSpin->setRange(1000, 60000);
    m_segmentDurationSpin->setSingleStep(1000);
    m_segmentDurationSpin->setSuffix(QStringLiteral(u" ms"));
    segRow->addWidget(m_segmentDurationSpin, 1);
    segRow->addStretch();
    form->addRow(QStringLiteral(u"\u5206\u6bb5\u65f6\u957f:"), segRow);

    // ===== 智能语义分段 =====
    m_semanticSegCheck = new QCheckBox(
        QStringLiteral(u"\u542f\u7528\u667a\u80fd\u8bed\u4e49\u5206\u6bb5 (\u6839\u636e\u8bed\u97f3+\u753b\u9762\u573a\u666f\u81ea\u9002\u5e94\u5207\u5206)"), this);
    form->addRow(QStringLiteral(u"\u5206\u6bb5\u7b56\u7565:"), m_semanticSegCheck);

    root->addLayout(form);

    // ===== 缓存组 =====
    QGroupBox* cacheGroup = new QGroupBox(QStringLiteral(u"\u5206\u6790\u7ed3\u679c\u7f13\u5b58"), this);
    QVBoxLayout* cacheLayout = new QVBoxLayout(cacheGroup);
    cacheLayout->setContentsMargins(12, 12, 12, 12);

    m_cacheEnabledCheck = new QCheckBox(
        QStringLiteral(u"\u542f\u7528\u7f13\u5b58 (\u540c\u4e00\u89c6\u9891\u7ecf\u5386\u540c\u540d/\u540c\u5927\u5c0f/\u540c\u4fee\u6539\u65f6\u6062\u590d\u4e0a\u6b21\u7ed3\u679c)"), this);
    cacheLayout->addWidget(m_cacheEnabledCheck);

    m_cacheSizeLabel = new QLabel(this);
    m_cacheSizeLabel->setStyleSheet(QStringLiteral(
        u"QLabel { color: #6B7280; font-size: 12px; }"));
    cacheLayout->addWidget(m_cacheSizeLabel);

    QHBoxLayout* cacheBtnRow = new QHBoxLayout();
    m_clearCacheBtn = new QPushButton(QStringLiteral(u"\U0001F5D1 \u6e05\u7a7a\u7f13\u5b58"), this);
    cacheBtnRow->addWidget(m_clearCacheBtn);
    cacheBtnRow->addStretch();
    cacheLayout->addLayout(cacheBtnRow);

    root->addWidget(cacheGroup);

    root->addStretch();

    // ===== 底部按钮 =====
    QHBoxLayout* bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    QPushButton* cancelBtn = new QPushButton(QStringLiteral(u"\u53d6\u6d88"), this);
    QPushButton* confirmBtn = new QPushButton(QStringLiteral(u"\u4fdd\u5b58"), this);
    confirmBtn->setDefault(true);
    bottomRow->addWidget(cancelBtn);
    bottomRow->addWidget(confirmBtn);
    root->addLayout(bottomRow);

    // 信号
    connect(m_apiKeyShowBtn, &QToolButton::toggled,
            this, &SummarySettingsDialog::onApiKeyShowToggled);
    connect(m_clearCacheBtn, &QPushButton::clicked,
            this, &SummarySettingsDialog::onCacheClearClicked);
    connect(confirmBtn, &QPushButton::clicked,
            this, &SummarySettingsDialog::onConfirmClicked);
    connect(cancelBtn, &QPushButton::clicked,
            this, &SummarySettingsDialog::onCancelClicked);
}

void SummarySettingsDialog::loadFromConfig() {
    ConfigManager& cfg = ConfigManager::instance();
    m_apiKeyLine->setText(cfg.getSummaryApiKey());
    m_endpointLine->setText(cfg.getSummaryModelEndpoint());
    int idx = m_modelCombo->findData(cfg.getSummaryModel());
    m_modelCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_segmentDurationSpin->setValue(cfg.getSummarySegmentDuration());
    m_semanticSegCheck->setChecked(cfg.getSemanticSegmentationEnabled());
    m_cacheEnabledCheck->setChecked(cfg.getSummaryCacheEnabled());
}

void SummarySettingsDialog::onApiKeyShowToggled(bool checked) {
    m_apiKeyLine->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

void SummarySettingsDialog::updateCacheSizeLabel() {
    qint64 bytes = VideoSummaryManager::cacheTotalSize();
    int files = VideoSummaryManager::cacheFileCount();
    QString sizeStr;
    if (bytes < 1024) sizeStr = QString::number(bytes) + " B";
    else if (bytes < 1024 * 1024) sizeStr = QString::number(bytes / 1024.0, 'f', 1) + " KB";
    else sizeStr = QString::number(bytes / 1024.0 / 1024.0, 'f', 2) + " MB";
    m_cacheSizeLabel->setText(
        QStringLiteral(u"\u5f53\u524d\u7f13\u5b58: %1 / %2 \u4e2a\u89c6\u9891")
            .arg(sizeStr).arg(files));
}

void SummarySettingsDialog::onCacheClearClicked() {
    int n = VideoSummaryManager::cacheFileCount();
    auto ret = QMessageBox::question(
        this, QStringLiteral(u"\u6e05\u7a7a\u7f13\u5b58"),
        QStringLiteral(u"\u786e\u5b9a\u8981\u5220\u9664\u5168\u90e8 %1 \u4e2a\u7f13\u5b58\u6587\u4ef6\u5417\uff1f\n\u4e0b\u6b21\u5206\u6790\u89c6\u9891\u9700\u91cd\u65b0\u8c03\u7528 LLM/VLM\u3002").arg(n),
        QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        VideoSummaryManager::clearAllCache();
        updateCacheSizeLabel();
        QMessageBox::information(this, QStringLiteral(u"\u6e05\u7a7a\u7f13\u5b58"),
                                 QStringLiteral(u"\u7f13\u5b58\u5df2\u6e05\u7a7a"));
    }
}

void SummarySettingsDialog::onConfirmClicked() {
    ConfigManager& cfg = ConfigManager::instance();
    cfg.setSummaryApiKey(m_apiKeyLine->text().trimmed());
    cfg.setSummaryModelEndpoint(m_endpointLine->text().trimmed());
    cfg.setSummaryModel(m_modelCombo->currentData().toString());
    cfg.setSummarySegmentDuration(m_segmentDurationSpin->value());
    cfg.setSemanticSegmentationEnabled(m_semanticSegCheck->isChecked());
    cfg.setSummaryCacheEnabled(m_cacheEnabledCheck->isChecked());
    cfg.save();
    accept();
}

void SummarySettingsDialog::onCancelClicked() {
    reject();
}
