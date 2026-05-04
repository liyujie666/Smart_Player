#ifndef SUBTITLEPOPUP_H
#define SUBTITLEPOPUP_H

#include <QWidget>

class QPushButton;

class SubtitlePopup : public QWidget
{
    Q_OBJECT
public:
    explicit SubtitlePopup(QWidget *parent = nullptr);

    QPushButton *realtimeBtn_;
    QPushButton *translateBtn_;
};

#endif // SUBTITLEPOPUP_H
