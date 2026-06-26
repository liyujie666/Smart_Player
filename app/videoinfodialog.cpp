#include "videoinfodialog.h"
#include "ui_videoinfodialog.h"
#include <QDebug>

VideoInfoDialog::VideoInfoDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::VideoInfoDialog)
{
    ui->setupUi(this);
    setWindowTitle("视频信息");
    this->setStyleSheet("QDialog{background-color:black;}QLabel{background-color:black;color:white;}");
}

VideoInfoDialog::~VideoInfoDialog()
{
    delete ui;
}

void VideoInfoDialog::updateinformation(const MediaInfo& info)
{
    // 时间格式化
    int totalTime = int(info.durationMs / 1000);
    QLatin1Char fill = QLatin1Char('0');
    QString totalTimeStr = QString("%1:%2:%3")
        .arg(totalTime / 3600,        2, 10, fill)
        .arg((totalTime / 60) % 60,    2, 10, fill)
        .arg(totalTime % 60,           2, 10, fill);

    ui->fileNameLabel->setText("文件名: " + info.fileName);
    ui->durationLabel->setText("总时长: " + totalTimeStr);
    ui->nameLabel->setText("封装格式名称: " + info.formatName);

    QString pixFmtStr = info.hasVideo
        ? (info.videoPixelFormat.isEmpty() ? QStringLiteral("未知") : info.videoPixelFormat)
        : QStringLiteral("无视频数据");
    ui->picFmtLabel->setText("视频像素格式: " + pixFmtStr);

    ui->bitRateLabel->setText(QStringLiteral("码率: ") +
        (info.bitRate > 0 ? QString::number(info.bitRate) : QStringLiteral("未知")));

    if (!info.hasAudio) {
        ui->aChannelsLabel->setText("音频声道数: 无音频数据");
        ui->aSampleRateLabel->setText("音频采样率: 无音频数据");
    } else {
        ui->aChannelsLabel->setText("音频声道数: " + QString::number(info.audioChannels));
        ui->aSampleRateLabel->setText("音频采样率: " + QString::number(info.audioSampleRate) + "Hz");
    }
    ui->frameRateLabel->setText(QString("帧率: %1 fps").arg(info.videoFrameRate));
}
