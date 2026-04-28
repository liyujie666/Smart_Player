#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>

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


private slots:
    void on_hardwareRadio_toggled(bool checked);
    void on_softwareRadio_toggled(bool checked);
    void on_selectPathBtn_clicked();
    void on_SizeModeChanged(int mode);
    void on_lightSlider_valueChanged(int value);
    void on_contrastSlider_valueChanged(int value);
    void on_baoheSlider_valueChanged(int value);

signals:
    void startHardWareAccep(bool on);
    void startSoftWareAccep(bool on);
    void updateSaveFilePath(QString path);
    void updateVideoSizeMode(int id);
    void updateUserDecoder(const QString decoder);
    void brightnessValueChanged(int value);
    void contrastValueChanged(int value);
    void saturationValueChanged(int value);
private:
    Ui::settingDialog *ui;
};

#endif // SETTINGDIALOG_H
