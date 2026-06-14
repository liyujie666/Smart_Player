#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>

namespace Ui {
class settingDialog;
}

class settingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit settingDialog(QWidget *parent = nullptr);
    ~settingDialog();


    bool getIsHardWare() const;
    QString getDecoderFormat() const;
    int getBrightness() const;
    QString getVideoSizeForm() const;
    QString getSavePath() const;
    QString getModelPath() const;


private slots:
    void on_hardwareRadio_toggled(bool checked);
    void on_softwareRadio_toggled(bool checked);
    void on_selectPathBtn_clicked();
    void on_SizeModeChanged(int mode);
    void on_lightSlider_valueChanged(int value);
    void on_contrastSlider_valueChanged(int value);
    void on_baoheSlider_valueChanged(int value);
    void on_uploadModelPathBtn_clicked();
    void on_cancelBtn_clicked();
    void on_confirmBtn_clicked();

    void on_resetConfigBtn_clicked();

    // AI 总结配置
    void on_summaryApiKeyChanged(const QString& text);
    void on_summaryEndpointChanged(const QString& text);
    void on_summarySegmentDurationChanged(int value);
    void on_summaryModelChanged(const QString& model);
    void on_summarySemanticSegChanged(bool checked);

signals:
    void startHardWareAccep(bool on);
    void startSoftWareAccep(bool on);
    void updateSaveFilePath(QString path);
    void updateVideoSizeMode(int id);
    void updateUserDecoder(const QString decoder);
    void brightnessValueChanged(int value);
    void contrastValueChanged(int value);
    void saturationValueChanged(int value);
    void updateModelPath(const QString& path);
private:
    Ui::settingDialog *ui;
    bool originalHardware_;
    int originalSizeMode_;
    int originalBrightness_;
    int originalContrast_;
    int originalSaturation_;
    QString originalScreenshotPath_;
    QString originalModelPath_;

    // AI 总结配置控件（动态创建，不通过 .ui）
    QLineEdit* m_summaryApiKeyLine = nullptr;
    QLineEdit* m_summaryEndpointLine = nullptr;
    QSpinBox* m_summarySegmentDurationSpin = nullptr;
    QLabel* m_summarySegmentDurationLabel = nullptr;
    QComboBox* m_summaryModelCombo = nullptr;
    QCheckBox* m_summarySemanticSegCheck = nullptr;
};

#endif // SETTINGDIALOG_H
