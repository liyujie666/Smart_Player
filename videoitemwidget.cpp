#include "videoitemwidget.h"
#include "ui_videoitemwidget.h"
#include <QPixmap>

VideoItemWidget::VideoItemWidget(const QImage &preview, const QString &name, const QString &duration, QWidget *parent)
    : QWidget(parent), ui(new Ui::VideoItemWidget)
{
    ui->setupUi(this);
    setMouseTracking(true);
    ui->fileNameLabel->setWordWrap(false);  // 不自动换
    ui->fileNameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(1); // 防止 auto size 收缩

    ui->previewImageLabel->setPixmap(QPixmap::fromImage(preview).scaled(110, 65, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->fileNameLabel->setText(name);
    ui->durationLabel->setText(duration);
}

VideoItemWidget::~VideoItemWidget()
{
    delete ui;
}

void VideoItemWidget::setFileNameTextColor(const QColor &color)
{
    QString style = QString("color: rgb(%1, %2, %3);")
    .arg(color.red()).arg(color.green()).arg(color.blue());
    ui->fileNameLabel->setStyleSheet(style);
}

void VideoItemWidget::enterEvent(QEnterEvent *event)
{
    this->setStyleSheet(" background: rgba(100, 100, 100, 100);");  // 鼠标悬停颜色
    QWidget::enterEvent(event);
}

void VideoItemWidget::leaveEvent(QEvent *event)
{
    this->setStyleSheet("");  // 恢复默认样式
    QWidget::leaveEvent(event);
}
