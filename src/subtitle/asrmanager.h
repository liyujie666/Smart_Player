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

    void setModelPath(const QString& path);
    bool isModelPathEmpty() const { return model_path_.isEmpty(); }

    // 预加载模型：在后台线程加载好模型，不阻塞 UI
    // 后续播放时 init() 会直接使用已缓存的上下文
    void warmUp();

private:
    void switchMode(Demuxer::MediaType type);

private:
    QString model_path_;
    SubtitleQueue queue_;
    std::unique_ptr<IAsrStrategy> strategy_;
    Demuxer::MediaType last_type_ = Demuxer::MediaType::FILE_TYPE;
};

#endif
