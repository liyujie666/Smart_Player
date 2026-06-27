#ifndef VIDEOINFODIALOG_H
#define VIDEOINFODIALOG_H

#include <QDialog>
#include "viewmodel/playerviewmodel.h"  // MediaInfo DTO

namespace Ui {
class VideoInfoDialog;
}

// MVVM 阶段 5：VideoInfoDialog 不再直接持有 AVFormatContext 指针，
// 改为接受由 PlayerViewModel 提炼好的 MediaInfo DTO。
class VideoInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VideoInfoDialog(QWidget *parent = nullptr);
    ~VideoInfoDialog();

    void updateinformation(const MediaInfo& info);

private:
    Ui::VideoInfoDialog *ui;
};

#endif // VIDEOINFODIALOG_H
