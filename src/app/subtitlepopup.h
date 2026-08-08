#ifndef SUBTITLEPOPUP_H
#define SUBTITLEPOPUP_H

#include <QWidget>
#include <QLabel>
class QPushButton;
class QSlider;

class SubtitlePopup : public QWidget
{
    Q_OBJECT
public:
    explicit SubtitlePopup(QWidget *parent = nullptr);

    QPushButton *realtimeBtn_;
    QPushButton *translateBtn_;
    QPushButton *showOriginalBtn_;
    QSlider *fontSizeSlider_;
    QLabel *fontSizeLabel_;

signals:
    void fontSizeChanged(int size);
};

#endif // SUBTITLEPOPUP_H
