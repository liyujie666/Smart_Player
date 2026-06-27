#include "videoinfodialog.h"
#include "ui_videoinfodialog.h"
#include <QFileInfo>
#include <QDebug>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

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

void VideoInfoDialog::updateinformation(AVFormatContext *update_fmtCtx, const char *filename)
{
    int audioIndex = -1;
    int videoIndex = -1;
    for(int i=0;i < update_fmtCtx->nb_streams;i++){
        if(update_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            videoIndex = i;
        }else if(update_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audioIndex = i;
        }
    }
    fmtCtx_ = update_fmtCtx;

    double frameRate = 0.0;
    if (videoIndex >= 0 && update_fmtCtx->streams[videoIndex]) {
        AVRational framerate = update_fmtCtx->streams[videoIndex]->codecpar->framerate;
        if (framerate.den > 0) {
            frameRate = (double)framerate.num / (double)framerate.den;
        }
    }

    QFileInfo fileInfo(filename);
    QString fileName_ = fileInfo.fileName();

    int64_t duration = fmtCtx_->duration;
    if (duration == AV_NOPTS_VALUE) duration = 0;
    int totalTime = round(duration * av_q2d(AV_TIME_BASE_Q));
    QLatin1Char fill = QLatin1Char('0');
    QString totalTimeStr = QString("%1:%2:%3")
        .arg(totalTime/3600,2,10,fill)
        .arg((totalTime/60)%60,2,10,fill)
        .arg(totalTime%60,2,10,fill);

    ui->fileNameLabel->setText("文件名: "+fileName_);
    ui->durationLabel->setText("总时长: "+totalTimeStr);
    ui->nameLabel->setText("封装格式名称: "+QString(update_fmtCtx->iformat->name));

    QString pixFmtStr;
    if (videoIndex == -1) {
        pixFmtStr = "无视频数据";
    } else {
        AVPixelFormat pixFmt = (AVPixelFormat)update_fmtCtx->streams[videoIndex]->codecpar->format;
        const char* name = av_get_pix_fmt_name(pixFmt);
        pixFmtStr = name ? QString(name) : "未知";
    }
    ui->picFmtLabel->setText("视频像素格式: " + pixFmtStr);

    int64_t bitRate = fmtCtx_->bit_rate;
    if (bitRate <= 0 || bitRate == AV_NOPTS_VALUE) bitRate = 0;
    ui->bitRateLabel->setText("码率: " + (bitRate > 0 ? QString::number(bitRate) : "未知"));

    if (audioIndex == -1) {
        ui->aChannelsLabel->setText("音频声道数: 无音频数据");
        ui->aSampleRateLabel->setText("音频采样率: 无音频数据");
    } else {
        ui->aChannelsLabel->setText("音频声道数: " + QString::number(update_fmtCtx->streams[audioIndex]->codecpar->ch_layout.nb_channels));
        ui->aSampleRateLabel->setText("音频采样率: " + QString::number(update_fmtCtx->streams[audioIndex]->codecpar->sample_rate) + "Hz");
    }
    ui->frameRateLabel->setText(QString("帧率: %1 fps").arg(frameRate));
}
