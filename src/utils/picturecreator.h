#ifndef PICTURECREATOR_H
#define PICTURECREATOR_H
#include <QImage>
#include <string>
extern"C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
class PictureCreator
{
public:
    PictureCreator();
    ~PictureCreator();

    QImage getPreViewImage(const QString &videoPath,int maxWidth = 120,int maxHeight = 90);
    int duration();
    int duration(const QString &videoPath);
    QString getFileType(const QString &videoPath);
private:
    QImage convertFrameToQImage(AVFrame *frame, int maxWidth, int maxHeight);
    int duration_ = -1;
};

#endif // PICTURECREATOR_H
