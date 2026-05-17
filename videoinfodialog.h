#ifndef VIDEOINFODIALOG_H
#define VIDEOINFODIALOG_H

#include <QDialog>
extern "C" {
#include <libavformat/avformat.h>
}

namespace Ui {
class VideoInfoDialog;
}

class VideoInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VideoInfoDialog(QWidget *parent = nullptr);
    ~VideoInfoDialog();

    void updateinformation(AVFormatContext *update_fmtCtx,const char *filename);

private:
    Ui::VideoInfoDialog *ui;
    AVFormatContext *fmtCtx_ = nullptr;
};

#endif // VIDEOINFODIALOG_H
