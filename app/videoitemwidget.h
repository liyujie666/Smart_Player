#ifndef VIDEOITEMWIDGET_H
#define VIDEOITEMWIDGET_H
#include <QEnterEvent>
#include <QWidget>
#include <QImage>

namespace Ui {
class VideoItemWidget;
}

class VideoItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoItemWidget(const QImage &preview, const QString &name, const QString &duration, QWidget *parent = nullptr);
    ~VideoItemWidget();

    void setFileNameTextColor(const QColor &color);
    void updateThumbnail(const QImage &preview);
protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
private:
    Ui::VideoItemWidget *ui;
};


#endif // VIDEOITEMWIDGET_H
