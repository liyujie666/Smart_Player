#ifndef PREVIEWPLAYER_H
#define PREVIEWPLAYER_H

#include <QObject>
#include <QByteArray>
#include <QMutex>
#include <QThread>
#include "demuxer/demuxer.h"
#include "decoder/decoder.h"

extern "C" {
#include <libavformat/avformat.h>
}

class PreviewPlayer : public QObject
{
    Q_OBJECT
public:
    explicit PreviewPlayer(QObject *parent = nullptr);
    ~PreviewPlayer();

    bool open(const QString& filePath);
    void requestPreview(int64_t seekTimeSec);
    void stop();

signals:
    void previewFrameReady(const QByteArray& data, int w, int h, AVPixelFormat fmt);

private:
    void decode(int64_t seekTimeSec);
    void release();

private:

    Demuxer*    demuxer_    = nullptr;
    Decoder*    decoder_    = nullptr;
    QString     currentFile_;

    QThread*    workThread_;

    QMutex      request_mutex_;
    int64_t     latest_seek_time_ = -1;
};

#endif // PREVIEWPLAYER_H
