#include "videosummarymanager.h"
#include "videosummarysegmenter.h"
#include "subtitle/asrworker.h"
#include "demuxer/demuxer.h"
#include "decoder/decoder.h"
#include "resampler/resampler.h"
#include "configmanager.h"
#include <QProcess>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QDeadlineTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>

VideoSummaryManager::VideoSummaryManager(QObject* parent)
    : QObject(parent)
{
    m_networkClient = new SummaryNetworkClient();

    ConfigManager& cfg = ConfigManager::instance();
    m_networkClient->setApiKey(cfg.getSummaryApiKey());
    m_networkClient->setBaseUrl(cfg.getSummaryModelEndpoint());
    m_networkClient->setModel(cfg.getSummaryModel().isEmpty()
                              ? QStringLiteral(u"qwen-vl-plus")
                              : cfg.getSummaryModel());
}

void VideoSummaryManager::setModel(const QString& model) {
    m_networkClient->setModel(model);
}

VideoSummaryManager::~VideoSummaryManager() {
    stopSummary();
    cleanupTempFiles();
    delete m_networkClient;
}

void VideoSummaryManager::setState(SummaryState s) {
    m_state = s;
    emit stateChanged(s);
}

void VideoSummaryManager::startSummary(const QString& videoPath) {
    qDebug() << "[Summary] ========== 启动视频总结 ==========";
    qDebug() << "[Summary] 视频路径:" << videoPath;

    // 如果有分析正在进行，先停止
    if (m_state != SummaryState::Idle && m_state != SummaryState::Finished
        && m_state != SummaryState::Error) {
        stopSummary();
    }

    m_videoPath = videoPath;
    m_stopRequested = false;
    m_currentSegment = 0;
    m_segments.clear();
    m_asrResults.clear();
    m_extractedFrames.clear();
    cleanupTempFiles();

    m_workerThread = new QThread(this);
    m_networkBridge = new SummaryNetworkBridge(m_networkClient, m_workerThread);
    moveToThread(m_workerThread);
    m_workerThread->start();

    QMetaObject::invokeMethod(this, "runAnalysis", Qt::QueuedConnection);
}

void VideoSummaryManager::stopSummary() {
    if (m_state == SummaryState::Idle || m_state == SummaryState::Finished
        || m_state == SummaryState::Error) {
        return;
    }

    m_stopRequested = true;
    setState(SummaryState::Stopping);
    m_networkClient->abortAll();

    cleanupTempFiles();

    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
        delete m_workerThread;
        m_workerThread = nullptr;
        delete m_networkBridge;
        m_networkBridge = nullptr;
    }

    setState(SummaryState::Idle);
}

void VideoSummaryManager::cleanupTempFiles() {
    for (const QString& f : m_tempFiles) {
        QFile::remove(f);
    }
    m_tempFiles.clear();
    m_extractedFrames.clear();
}

VideoSummaryManager::Progress VideoSummaryManager::progress() const {
    Progress p;
    p.stage = m_state;
    p.currentSegment = m_currentSegment;
    p.totalSegments = m_segments.size();

    switch (m_state) {
        case SummaryState::Idle:
            p.stageProgress = 0.0;
            p.overallProgress = 0.0;
            break;
        case SummaryState::ExtractingFrames: {
            int totalFrames = qMax(1, m_segments.size() * 2);
            p.stageProgress = m_extractedFrames.size() / static_cast<double>(totalFrames);
            p.overallProgress = p.stageProgress * 0.3;
            break;
        }
        case SummaryState::RunningASR:
            p.stageProgress = 0.5;
            p.overallProgress = 0.3 + p.stageProgress * 0.1;
            break;
        case SummaryState::AnalyzingSegments:
            if (m_segments.isEmpty()) {
                p.stageProgress = 0.0;
                p.overallProgress = 0.4;
            } else {
                p.stageProgress = (m_currentSegment + 1)
                    / static_cast<double>(m_segments.size());
                p.overallProgress = 0.4 + p.stageProgress * 0.5;
            }
            break;
        case SummaryState::Finished:
            p.stageProgress = 1.0;
            p.overallProgress = 1.0;
            break;
        case SummaryState::Stopping:
        case SummaryState::Error:
            p.stageProgress = 0.0;
            p.overallProgress = 0.0;
            break;
    }
    return p;
}

const SummarySegment* VideoSummaryManager::segmentAt(int index) const {
    if (index < 0 || index >= m_segments.size()) return nullptr;
    return &m_segments[index];
}

void VideoSummaryManager::runAnalysis() {
    qDebug() << "[Summary] 开始分析:" << m_videoPath;

    // 获取视频时长
    Demuxer durationProbe;
    if (durationProbe.open(m_videoPath.toStdString().c_str()) >= 0) {
        // getDuration() 返回秒，转为毫秒
        m_durationMs = durationProbe.getDuration() * 1000;
        qDebug() << "[Summary] 视频时长:" << m_durationMs << "ms (" << (m_durationMs / 1000) << "s)";
        durationProbe.close();
    }

    if (m_durationMs <= 0) {
        emit errorOccurred(QStringLiteral(u"无法获取视频时长"));
        setState(SummaryState::Error);
        return;
    }

    int segDuration = ConfigManager::instance().getSummarySegmentDuration();
    qDebug() << "[Summary] 分段时长:" << segDuration << "ms";
    m_segments = SummarySegmenter::segmentByDuration(m_durationMs, segDuration);
    qDebug() << "[Summary] 分段数量:" << m_segments.size() << "段";

    setState(SummaryState::ExtractingFrames);
    emit progressDetailChanged(progress());

    qDebug() << "[Summary] 阶段: 提取关键帧, 共需提取" << (m_segments.size() * 2) << "帧";
    extractFrames();
    qDebug() << "[Summary] 帧提取完成, 成功提取" << m_extractedFrames.size() << "帧";
    if (m_stopRequested) return;

    setState(SummaryState::RunningASR);
    emit progressDetailChanged(progress());

    qDebug() << "[Summary] 阶段: Whisper ASR 语音识别";
    QString audioPath = extractAudioFile(m_videoPath);
    qDebug() << "[Summary] 音频提取完成, 临时文件:" << audioPath;
    if (!audioPath.isEmpty()) {
        runWhisperASR(audioPath);
        qDebug() << "[Summary] ASR 完成, 识别到" << m_asrResults.size() << "条语音";
        QFile::remove(audioPath);
        m_tempFiles.removeAll(audioPath);
    } else {
        qDebug() << "[Summary] 音频提取失败，跳过 ASR";
    }

    if (m_stopRequested) return;

    if (!m_asrResults.isEmpty()) {
        SummarySegmenter::aggregateSpeechText(m_segments, m_asrResults);
        qDebug() << "[Summary] 语音聚合完成";
    }

    setState(SummaryState::AnalyzingSegments);
    emit progressDetailChanged(progress());

    qDebug() << "[Summary] 阶段: 逐段 VLM 分析, 共" << m_segments.size() << "段";
    analyzeSegments();
    qDebug() << "[Summary] 段落分析完成";
    if (m_stopRequested) return;

    qDebug() << "[Summary] 阶段: 生成完整报告";
    generateFullReport();
    qDebug() << "[Summary] 完整报告生成完成";

    cleanupTempFiles();

    setState(SummaryState::Finished);
    emit progressDetailChanged(progress());
    qDebug() << "[Summary] 分析全部完成";
}

void VideoSummaryManager::extractFrames() {
    if (m_segments.isEmpty()) return;

    int totalFramesNeeded = m_segments.size() * 2;
    for (int i = 0; i < totalFramesNeeded; ++i) {
        if (m_stopRequested) return;

        qint64 targetMs = (i * m_durationMs) / totalFramesNeeded;
        QByteArray jpegData = extractFrameJpeg(m_videoPath, targetMs);
        if (!jpegData.isEmpty()) {
            m_extractedFrames[targetMs] = jpegData;
        } else {
            qDebug() << "[Summary] 帧提取失败 at" << targetMs << "ms";
        }

        if (i % 10 == 0) {
            emit progressDetailChanged(progress());
        }
    }
}

QByteArray VideoSummaryManager::extractFrameJpeg(const QString& videoPath, qint64 timestampMs) {
    QProcess proc;
    proc.setProgram("ffmpeg");

    QString timeStr = QString::number(timestampMs / 1000.0, 'f', 3);
    QStringList args;
    args << "-y"
         << "-ss" << timeStr
         << "-i" << videoPath
         << "-vframes" << "1"
         << "-q:v" << "2"
         << "-f" << "image2pipe"
         << "-vcodec" << "mjpeg"
         << "pipe:1";
    proc.setArguments(args);

    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(QProcess::ReadOnly);

    if (!proc.waitForFinished(5000)) {
        proc.kill();
        return {};
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        return {};
    }

    return proc.readAllStandardOutput();
}

QString VideoSummaryManager::extractAudioFile(const QString& videoPath) {
    QString tempAudio = QDir::temp().absoluteFilePath(
        "smartplayer_asr_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".wav");
    m_tempFiles.append(tempAudio);

    QProcess proc;
    proc.setProgram("ffmpeg");
    QStringList args;
    args << "-y"
         << "-i" << videoPath
         << "-vn"
         << "-acodec" << "pcm_s16le"
         << "-ar" << "16000"
         << "-ac" << "1"
         << tempAudio;
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(QProcess::ReadOnly);

    if (!proc.waitForFinished(30000)) {
        proc.kill();
        return {};
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        return {};
    }

    return tempAudio;
}

void VideoSummaryManager::runWhisperASR(const QString& audioPath) {
    qDebug() << "[Summary] ASR 开始, 模型路径:" << ConfigManager::instance().getModelPath();
    std::unique_ptr<Demuxer> demux = std::make_unique<Demuxer>();
    if (demux->open(audioPath.toStdString().c_str()) < 0) {
        emit errorOccurred(QStringLiteral(u"无法打开音频文件进行ASR"));
        return;
    }

    auto as = demux->getStream(AVMEDIA_TYPE_AUDIO);
    if (!as) {
        emit errorOccurred(QStringLiteral(u"音频流不存在"));
        return;
    }

    std::unique_ptr<Decoder> dec = std::make_unique<Decoder>();
    if (dec->init(as->codecpar, AVMEDIA_TYPE_AUDIO) < 0) {
        emit errorOccurred(QStringLiteral(u"无法初始化音频解码器"));
        return;
    }

    Resampler::AudioSpec inSpec, outSpec;
    inSpec.sampleRate = as->codecpar->sample_rate;
    inSpec.sampleFmt = (AVSampleFormat)as->codecpar->format;
    inSpec.chs = as->codecpar->ch_layout.nb_channels;
    av_channel_layout_copy(&inSpec.chLayout, &as->codecpar->ch_layout);

    outSpec.sampleRate = 16000;
    outSpec.sampleFmt = AV_SAMPLE_FMT_FLT;
    outSpec.chs = 1;
    av_channel_layout_from_string(&outSpec.chLayout, "mono");

    std::unique_ptr<Resampler> res = std::make_unique<Resampler>();
    if (res->init(inSpec, outSpec) < 0) {
        emit errorOccurred(QStringLiteral(u"无法初始化重采样器"));
        return;
    }

    AsrWorker worker;
    AsrConfig cfg;
    cfg.model_path = ConfigManager::instance().getModelPath().toStdString();
    if (cfg.model_path.empty()) {
        emit errorOccurred(QStringLiteral(u"ASR模型路径未设置"));
        return;
    }
    if (!worker.init(cfg)) {
        emit errorOccurred(QStringLiteral(u"无法初始化ASR模型"));
        return;
    }

    const int SEG = 30;
    const int SR = 16000;
    std::vector<float> pcm;
    double startSec = 0;

    demux->seek(0);
    dec->flush();

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    while (demux->readPacket(pkt) >= 0) {
        if (m_stopRequested) break;

        if (pkt->stream_index != demux->getStreamIndex(AVMEDIA_TYPE_AUDIO)) {
            av_packet_unref(pkt);
            continue;
        }

        if (dec->decode(pkt, frame) != 0) {
            av_packet_unref(pkt);
            continue;
        }

        uint8_t* buf = (uint8_t*)av_malloc(res->getOutputBufferSize(frame->nb_samples));
        int samples = 0;
        if (res->resample(frame, &buf, &samples) >= 0 && samples > 0) {
            pcm.insert(pcm.end(), (float*)buf, (float*)buf + samples);
        }
        av_free(buf);
        av_packet_unref(pkt);
        av_frame_unref(frame);

        if ((int)pcm.size() >= SEG * SR) {
            std::vector<SubtitleItem> subs;
            worker.recognize(pcm, subs, startSec);
            for (auto& s : subs) {
                m_asrResults.append(s);
            }
            startSec += SEG;
            pcm.clear();
        }
    }

    if (!pcm.empty() && !m_stopRequested) {
        std::vector<SubtitleItem> subs;
        worker.recognize(pcm, subs, startSec);
        for (auto& s : subs) {
            m_asrResults.append(s);
        }
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    worker.release();
}

void VideoSummaryManager::analyzeSegments() {
    qDebug() << "[Summary] 开始逐段 VLM 分析, 令牌桶并发上限: 5";

    connect(m_networkClient, &SummaryNetworkClient::frameAnalyzed,
            this, &VideoSummaryManager::onFrameAnalyzed);

    for (int i = 0; i < m_segments.size(); ++i) {
        if (m_stopRequested) {
            qDebug() << "[Summary] 检测到停止请求, 退出分析";
            disconnect(m_networkClient, &SummaryNetworkClient::frameAnalyzed,
                       this, &VideoSummaryManager::onFrameAnalyzed);
            return;
        }

        m_currentSegment = i;
        emit progressDetailChanged(progress());
        emit progressUpdated(progress().overallProgress);

        auto& seg = m_segments[i];
        qDebug() << "[Summary] 分析第" << (i + 1) << "/" << m_segments.size()
                 << "段:" << seg.startMs << "-" << seg.endMs << "ms";

        QString timeRange = QStringLiteral(u"%1-%2s")
            .arg(seg.startMs / 1000.0, 0, 'f', 1)
            .arg(seg.endMs / 1000.0, 0, 'f', 1);

        QList<qint64> timestamps = SummarySegmenter::sampleTimestamps(
            seg.startMs, seg.endMs, 2);

        QList<QByteArray> frameJpegs;
        for (qint64 ts : timestamps) {
            auto it = m_extractedFrames.lowerBound(ts);
            if (it != m_extractedFrames.end()) {
                frameJpegs.append(it.value());
            }
        }

        if (frameJpegs.isEmpty()) {
            seg.isAnalyzed = true;
            seg.description = QStringLiteral(u"(无画面数据)");
            qDebug() << "[Summary] 第" << (i + 1) << "段: 无画面数据，跳过";
            emit segmentAnalyzed(i, seg.description);
            emit progressUpdated(progress().overallProgress);
            continue;
        }

        qDebug() << "[Summary] 第" << (i + 1) << "段: 提交"
                 << frameJpegs.size() << "帧到 VLM 分析";

        m_pendingAnalyzes = frameJpegs.size();
        m_pendingMutex.lock();
        m_pendingFrameDescs.clear();
        m_pendingHasError = false;
        m_pendingErrorMsg.clear();

        for (const QByteArray& jpeg : frameJpegs) {
            QMetaObject::invokeMethod(m_networkBridge, "analyzeFrame",
                Qt::QueuedConnection,
                Q_ARG(int, i),
                Q_ARG(QByteArray, jpeg),
                Q_ARG(QString, seg.speechText),
                Q_ARG(QString, timeRange));
        }

        while (m_pendingAnalyzes > 0 && !m_stopRequested) {
            m_pendingMutex.unlock();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            m_pendingMutex.lock();
        }
        m_pendingMutex.unlock();

        if (m_stopRequested) {
            disconnect(m_networkClient, &SummaryNetworkClient::frameAnalyzed,
                       this, &VideoSummaryManager::onFrameAnalyzed);
            return;
        }

        if (!m_pendingHasError) {
            QString combined = m_pendingFrameDescs.join("\n");
            seg.description = combined;
            seg.isAnalyzed = true;
            qDebug() << "[Summary] 第" << (i + 1) << "段 VLM 分析成功, 结果:"
                     << (combined.size() > 100 ? combined.left(100) + "..." : combined);
            emit segmentAnalyzed(i, combined);
            emit progressUpdated(progress().overallProgress);
        } else {
            seg.description = QStringLiteral(u"(分析失败: %1)").arg(m_pendingErrorMsg);
            seg.isAnalyzed = true;
            qDebug() << "[Summary] 第" << (i + 1) << "段 VLM 分析失败:" << m_pendingErrorMsg;
            emit segmentAnalyzed(i, seg.description);
            emit progressUpdated(progress().overallProgress);
        }
    }

    disconnect(m_networkClient, &SummaryNetworkClient::frameAnalyzed,
               this, &VideoSummaryManager::onFrameAnalyzed);
    qDebug() << "[Summary] 段落分析完成";
}

void VideoSummaryManager::onFrameAnalyzed(int segmentIndex, const QString& description,
                                          bool hasError, const QString& errorMsg) {
    QMutexLocker locker(&m_pendingMutex);
    if (hasError) {
        m_pendingHasError = true;
        m_pendingErrorMsg = errorMsg;
    } else {
        m_pendingFrameDescs.append(description);
    }
    if (--m_pendingAnalyzes == 0) {
        m_pendingCond.wakeAll();
    }
}

void VideoSummaryManager::generateFullReport() {
    qDebug() << "[Summary] 生成完整报告中...";

    m_pendingReport.clear();
    m_pendingHasError = false;
    m_pendingErrorMsg.clear();
    m_pendingReportReceived = false;

    connect(m_networkClient, &SummaryNetworkClient::reportReady,
            this, &VideoSummaryManager::onReportReady);

    qDebug() << "[Summary] 通过 bridge 触发报告生成...";
    QMetaObject::invokeMethod(m_networkBridge, "generateFullReport",
        Qt::QueuedConnection,
        Q_ARG(QList<SummarySegment>, m_segments),
        Q_ARG(QList<SubtitleItem>, m_asrResults));

    qDebug() << "[Summary] 等待报告返回, 启动局部事件循环...";
    QEventLoop loop;
    m_reportLoop = &loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(60000);
    connect(&timer, &QTimer::timeout, [&]() {
        qDebug() << "[Summary] 报告等待超时";
        loop.quit();
    });
    timer.start();
    loop.exec();
    m_reportLoop = nullptr;
    qDebug() << "[Summary] 事件循环退出";

    disconnect(m_networkClient, &SummaryNetworkClient::reportReady,
               this, &VideoSummaryManager::onReportReady);

    if (m_pendingHasError) {
        qDebug() << "[Summary] 报告生成失败:" << m_pendingErrorMsg;
        emit errorOccurred(QStringLiteral(u"报告生成失败: %1").arg(m_pendingErrorMsg));
    } else {
        qDebug() << "[Summary] 报告生成成功, 长度:" << m_pendingReport.size();
        emit fullReportReady(m_pendingReport);
    }
}

void VideoSummaryManager::onReportReady(const QString& reportJson, bool hasError, const QString& errorMsg) {
    qDebug() << "[Summary] onReportReady 被调用, hasError=" << hasError;
    m_pendingReport = reportJson;
    m_pendingHasError = hasError;
    m_pendingErrorMsg = errorMsg;
    m_pendingReportReceived = true;

    if (!hasError && !reportJson.isEmpty()) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(reportJson.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError) {
            QJsonObject obj = doc.object();
            m_fullReport.tldr = obj["tldr"].toString();
            QJsonArray takeaways = obj["key_takeaways"].toArray();
            m_fullReport.keyTakeaways.clear();
            for (const QJsonValue& v : takeaways) {
                m_fullReport.keyTakeaways.append(v.toString());
            }
            QJsonArray entities = obj["entities"].toArray();
            m_fullReport.entities.clear();
            for (const QJsonValue& v : entities) {
                SummaryEntity e;
                e.name = v.toObject()["name"].toString();
                e.type = v.toObject()["type"].toString();
                QString ts = v.toObject()["first_mention"].toString();
                if (!ts.isEmpty()) {
                    QStringList parts = ts.split(':');
                    if (parts.size() == 2) {
                        int mins = parts[0].toInt();
                        int secs = qRound(parts[1].toDouble());
                        e.firstMentionMs = (mins * 60 + secs) * 1000;
                    }
                }
                m_fullReport.entities.append(e);
            }
            QJsonArray chapters = obj["chapters"].toArray();
            m_fullReport.chapters.clear();
            for (const QJsonValue& v : chapters) {
                SummaryChapter ch;
                QJsonObject c = v.toObject();
                auto parseTs = [](const QString& s) -> qint64 {
                    QStringList parts = s.split(':');
                    if (parts.size() == 2) {
                        int mins = parts[0].toInt();
                        int secs = qRound(parts[1].toDouble());
                        return (mins * 60 + secs) * 1000;
                    }
                    return 0LL;
                };
                ch.startMs = parseTs(c["start"].toString());
                ch.endMs = parseTs(c["end"].toString());
                ch.title = c["title"].toString();
                m_fullReport.chapters.append(ch);
            }
            m_fullReport.fullMarkdown = obj["markdown"].toString();
            m_fullReport.isValid = true;
            emit structuredReportReady(m_fullReport);
        } else {
            qDebug() << "[Summary] JSON 解析失败:" << err.errorString();
        }
        emit fullReportReady(reportJson);
    }

    if (m_reportLoop) {
        m_reportLoop->quit();
    }
}
