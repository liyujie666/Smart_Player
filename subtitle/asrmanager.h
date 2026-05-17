#ifndef ASRMANAGER_H
#define ASRMANAGER_H

#include <QObject>
#include <memory>
#include "queue/subtitlequeue.h"
#include "iasrstrategy.h"
#include "demuxer/demuxer.h"

class AsrManager : public QObject {
    Q_OBJECT
public:
    explicit AsrManager(QObject *parent = nullptr);
    ~AsrManager() override;

    bool init(const QString& url, Demuxer::MediaType type, AVStream* audio);
    void start();
    void stop();
    void reset();
    void sendAudioFrame(AVFrame* frame);
    SubtitleQueue* queue() { return &queue_; }

    void setModelPath(const QString& path) { model_path_ = path; }
    bool isModelPathEmpty() const { return model_path_.isEmpty(); }
private:
    void switchMode(Demuxer::MediaType type);

private:
    QString model_path_;
    SubtitleQueue queue_;
    std::unique_ptr<IAsrStrategy> strategy_;
    Demuxer::MediaType last_type_ = Demuxer::MediaType::FILE_TYPE;
};

#endif
