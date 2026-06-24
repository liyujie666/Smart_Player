#ifndef SUMMARYSETTINGSDIALOG_H
#define SUMMARYSETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QToolButton>
#include <QLabel>
#include <QPushButton>

// AI 总结专属设置对话框
// - 紧凑(不占用主设置对话框)
// - 风格匹配 SummaryPanel 浅色主题
// - 包含: API Key / 端点 / 模型 / 分段时长 / 语义分段开关 /
//         缓存开关 + 清空缓存
class SummarySettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SummarySettingsDialog(QWidget* parent = nullptr);

private slots:
    void onApiKeyShowToggled(bool checked);
    void onCacheClearClicked();
    void onConfirmClicked();
    void onCancelClicked();

private:
    void buildUI();
    void loadFromConfig();
    void updateCacheSizeLabel();

    // 输入
    QLineEdit*   m_apiKeyLine = nullptr;
    QToolButton* m_apiKeyShowBtn = nullptr;
    QLineEdit*   m_endpointLine = nullptr;
    QComboBox*   m_modelCombo = nullptr;
    QSpinBox*    m_segmentDurationSpin = nullptr;
    QCheckBox*   m_semanticSegCheck = nullptr;
    QCheckBox*   m_cacheEnabledCheck = nullptr;
    QPushButton* m_clearCacheBtn = nullptr;
    QLabel*      m_cacheSizeLabel = nullptr;
};

#endif // SUMMARYSETTINGSDIALOG_H
