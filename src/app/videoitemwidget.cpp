#include "videoitemwidget.h"
#include "ui_videoitemwidget.h"
#include <QFontMetrics>
#include <QResizeEvent>
#include <QPixmap>

VideoItemWidget::VideoItemWidget(const QImage &preview, const QString &name, const QString &duration, QWidget *parent)
    : QWidget(parent), ui(new Ui::VideoItemWidget), m_fullFileName(name)
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

    // 长文件名：保留完整名作为 tooltip + 根据宽度省略显示
    setToolTip(m_fullFileName);
    ui->fileNameLabel->setToolTip(m_fullFileName);
    updateFileNameElision();
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

void VideoItemWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateFileNameElision();
}

void VideoItemWidget::updateFileNameElision()
{
    if (m_fullFileName.isEmpty()) {
        return;
    }
    const int labelWidth = ui->fileNameLabel->width();
    if (labelWidth <= 0) {
        // 控件尚未布局完成，等下次 resizeEvent 再处理
        return;
    }
    const QFontMetrics fm(ui->fileNameLabel->font());
    const QString elided = fm.elidedText(m_fullFileName, Qt::ElideRight, labelWidth);
    if (ui->fileNameLabel->text() != elided) {
        ui->fileNameLabel->setText(elided);
    }
}

void VideoItemWidget::updateThumbnail(const QImage &preview)
{
    ui->previewImageLabel->setPixmap(QPixmap::fromImage(preview).scaled(110, 65, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
