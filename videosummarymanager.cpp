#include "videosummarymanager.h"
#include "videosummarysegmenter.h"
#include "semanticsegmenter.h"
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

    // 把 m_networkClient 和 this 放到主线程上,作为 moveToThread(worker) 之前
    // 的 "干净起点" 兜底。正常路径下 stopSummary 已经把它们拉回主线程了;
    // 这一行是双保险,处理 stopSummary 被绕过等边界情况。
    QThread* mainThread = QCoreApplication::instance()->thread();
    if (m_networkClient->thread() != mainThread) {
        m_networkClient->moveToThread(mainThread);
    }
    if (this->thread() != mainThread) {
        moveToThread(mainThread);
    }

    m_workerThread = new QThread(this);
    m_networkBridge = new SummaryNetworkBridge(m_networkClient, m_workerThread);
    m_networkClient->moveToThread(m_workerThread);
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
    m_sceneStopRequested = true;
    setState(SummaryState::Stopping);
    m_networkClient->abortAll();

    cleanupTempFiles();

    if (m_workerThread && m_workerThread->isRunning()) {
        // 在 delete QThread 之前,先把亲和性在 worker 线程上的对象
        // (m_networkClient 和 this) move 回主线程。否则 QThread 析构时
        // 会把它们的 thread() 置为 nullptr,导致下次 startSummary 时
        // moveToThread 报 "Current thread ... is not the object's thread (0x0)"。
        if (m_networkClient) m_networkClient->moveToThread(QCoreApplication::instance()->thread());
        this->moveToThread(QCoreApplication::instance()->thread());

        m_workerThread->quit();
        m_workerThread->wait(3000);
        // m_networkBridge 是以 m_workerThread 为父对象创建的,delete m_workerThread
        // 时 Qt 会自动 delete bridge。手动 delete 会 double-free 导致崩溃。
        delete m_workerThread;
        m_workerThread = nullptr;
        m_networkBridge = nullptr;   // 已被 Qt 自动 delete,仅置空指针
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
            int totalFrames = qMax(1, m_segments.isEmpty() ? qMax(1, int(m_durationMs / 2500)) : m_segments.size() * 2);
            p.stageProgress = m_extractedFrames.size() / static_cast<double>(totalFrames);
            p.overallProgress = p.stageProgress * 0.2;
            break;
        }
        case SummaryState::RunningASR:
            p.stageProgress = 0.5;
            p.overallProgress = 0.2 + p.stageProgress * 0.1;
            break;
        case SummaryState::ClassifyingScenes:
            p.stageProgress = 0.5;
            p.overallProgress = 0.3 + p.stageProgress * 0.05;
            break;
        case SummaryState::DetectingSemanticBoundaries:
            p.stageProgress = 0.5;
            p.overallProgress = 0.35 + p.stageProgress * 0.05;
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

void VideoSummaryManager::classifyVideoScenes() {
    const int SAMPLE_INTERVAL_MS = 15000;

    QList<QPair<qint64, QByteArray>> sampleFrames;
    for (qint64 ts = 0; ts < m_durationMs; ts += SAMPLE_INTERVAL_MS) {
        auto it = m_extractedFrames.lowerBound(ts);
        if (it != m_extractedFrames.end()) {
            sampleFrames.append({it.key(), it.value()});
        }
    }

    if (sampleFrames.isEmpty()) {
        qDebug() << "[Summary] 无帧可做场景分类";
        return;
    }

    m_sceneTags.clear();
    m_pendingSceneClassifications = sampleFrames.size();
    m_sceneStopRequested = false;

    connect(m_networkClient, &SummaryNetworkClient::sceneClassified,
            this, &VideoSummaryManager::onSceneClassified);

    for (const auto& frame : sampleFrames) {
        if (m_sceneStopRequested) break;
        QMetaObject::invokeMethod(m_networkBridge, "classifySingleScene",
            Qt::QueuedConnection,
            Q_ARG(qint64, frame.first),
            Q_ARG(QByteArray, frame.second));
    }

    while (m_pendingSceneClassifications > 0 && !m_sceneStopRequested) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }

    disconnect(m_networkClient, &SummaryNetworkClient::sceneClassified,
               this, &VideoSummaryManager::onSceneClassified);

    std::sort(m_sceneTags.begin(), m_sceneTags.end(),
               [](const QPair<qint64, QString>& a, const QPair<qint64, QString>& b) {
                   return a.first < b.first;
               });

    m_sceneTags = debounceSceneTags(m_sceneTags);

    for (const auto& tag : m_sceneTags) {
        qDebug() << "[Summary] 场景标签:" << tag.first << "ms ->" << tag.second;
    }
}

void VideoSummaryManager::onSceneClassified(qint64 timestampMs, const QString& sceneTag,
                                           bool hasError, const QString& errorMsg) {
    QMutexLocker locker(&m_sceneMutex);
    if (hasError) {
        qDebug() << "[Summary] 场景分类失败" << timestampMs << "ms:" << errorMsg;
        m_sceneTags.append({timestampMs, QString()});
    } else {
        m_sceneTags.append({timestampMs, sceneTag});
    }
    if (--m_pendingSceneClassifications == 0) {
        m_sceneCond.wakeAll();
    }
}

QList<QPair<qint64, QString>> VideoSummaryManager::debounceSceneTags(
    const QList<QPair<qint64, QString>>& rawTags) {
    if (rawTags.size() < 3) {
        return rawTags;
    }

    // 第 1 步：空标签填补 (失败帧用最近的非空标签)
    QList<QPair<qint64, QString>> filled;
    filled.reserve(rawTags.size());
    QString lastNonEmpty;
    for (const auto& p : rawTags) {
        if (p.second.isEmpty()) {
            if (!lastNonEmpty.isEmpty()) {
                filled.append({p.first, lastNonEmpty});
            } else {
                filled.append(p);
            }
        } else {
            filled.append(p);
            lastNonEmpty = p.second;
        }
    }
    // 尾部可能仍为空（开头就失败），用首个非空填补
    QString firstNonEmpty;
    for (const auto& p : filled) {
        if (!p.second.isEmpty()) { firstNonEmpty = p.second; break; }
    }
    if (!firstNonEmpty.isEmpty()) {
        for (auto& p : filled) {
            if (p.second.isEmpty()) p.second = firstNonEmpty;
        }
    }

    // 第 2 步：中值滤波 (前后各 K 帧的众数)
    const int K = 2;
    QList<QPair<qint64, QString>> smoothed;
    smoothed.reserve(filled.size());
    for (int i = 0; i < filled.size(); ++i) {
        QHash<QString, int> counter;
        int lo = qMax(0, i - K);
        int hi = qMin(filled.size() - 1, i + K);
        for (int j = lo; j <= hi; ++j) {
            const QString& t = filled[j].second;
            if (!t.isEmpty()) counter[t]++;
        }
        QString best;
        int bestCount = 0;
        for (auto it = counter.constBegin(); it != counter.constEnd(); ++it) {
            if (it.value() > bestCount) {
                best = it.key();
                bestCount = it.value();
            }
        }
        if (best.isEmpty()) best = filled[i].second;
        smoothed.append({filled[i].first, best});
    }

    // 第 3 步：最小停留时长去抖 (单点新标签视为噪声)
    // 找出所有"稳定段"，单帧段被两侧的稳定标签吸收
    QList<QPair<qint64, QString>> debounced = smoothed;
    for (int i = 1; i < debounced.size() - 1; ++i) {
        if (debounced[i].second != debounced[i - 1].second
            && debounced[i].second != debounced[i + 1].second
            && debounced[i - 1].second == debounced[i + 1].second) {
            debounced[i].second = debounced[i - 1].second;
        }
    }

    qDebug() << "[Summary] 场景标签去抖: 原始" << rawTags.size()
             << "个, 去抖后" << debounced.size() << "个";

    int beforeChanges = 0;
    for (int i = 1; i < smoothed.size(); ++i) {
        if (smoothed[i].second != smoothed[i - 1].second) ++beforeChanges;
    }
    int afterChanges = 0;
    for (int i = 1; i < debounced.size(); ++i) {
        if (debounced[i].second != debounced[i - 1].second) ++afterChanges;
    }
    qDebug() << "[Summary] 场景切换次数: 去抖前" << beforeChanges
             << "次, 去抖后" << afterChanges << "次";

    return debounced;
}

void VideoSummaryManager::runSemanticSegmentation() {
    SemanticSegmenter segmenter;
    SemanticSegmenter::Config segCfg;

    segCfg.audioWeight = ConfigManager::instance().getSemanticAudioWeight();
    segCfg.videoWeight = ConfigManager::instance().getSemanticVideoWeight();
    segCfg.minSegmentMs = ConfigManager::instance().getSemanticMinSegmentMs();
    segCfg.maxSegmentMs = ConfigManager::instance().getSemanticMaxSegmentMs();

    segmenter.setConfig(segCfg);

    QList<qint64> boundaries = segmenter.computeSegments(
        m_asrResults, m_durationMs, m_sceneTags);

    SemanticSegmenter::boundariesToSegments(boundaries, m_segments);

    qDebug() << "[Summary] 语义分段边界:" << boundaries;
    qDebug() << "[Summary] 检测到的边界详情:";
    for (const auto& sb : segmenter.allBoundaries()) {
        qDebug() << "  ->" << sb.timestampMs << "ms"
                 << "audio=" << sb.audioScore
                 << "video=" << sb.videoScore
                 << "reason:" << sb.audioReason << sb.videoReason;
    }
}

const SummarySegment* VideoSummaryManager::segmentAt(int index) const {
    if (index < 0 || index >= m_segments.size()) return nullptr;
    return &m_segments[index];
}

void VideoSummaryManager::runAnalysis() {
    qDebug() << "[Summary] 开始分析:" << m_videoPath;

    Demuxer durationProbe;
    if (durationProbe.open(m_videoPath.toStdString().c_str()) >= 0) {
        m_durationMs = durationProbe.getDuration() * 1000;
        qDebug() << "[Summary] 视频时长:" << m_durationMs << "ms (" << (m_durationMs / 1000) << "s)";
        durationProbe.close();
    }

    if (m_durationMs <= 0) {
        emit errorOccurred(QStringLiteral(u"无法获取视频时长"));
        setState(SummaryState::Error);
        return;
    }

    setState(SummaryState::ExtractingFrames);
    emit progressDetailChanged(progress());

    qDebug() << "[Summary] 阶段: 提取关键帧";
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

    bool useSemanticSeg = ConfigManager::instance().getSemanticSegmentationEnabled();
    int asrCount = m_asrResults.size();
    int sceneTagCount = m_sceneTags.size();
    qDebug() << "[Summary] === 分段决策诊断 ===";
    qDebug() << "[Summary] 语义分段开关:" << useSemanticSeg;
    qDebug() << "[Summary] ASR 语音条数:" << asrCount;
    qDebug() << "[Summary] 场景标签数:" << sceneTagCount;
    qDebug() << "[Summary] 固定分段时长设置:" << ConfigManager::instance().getSummarySegmentDuration() << "ms";
    qDebug() << "[Summary] 语义最小段长:" << ConfigManager::instance().getSemanticMinSegmentMs() << "ms";
    qDebug() << "[Summary] 语义最大段长:" << ConfigManager::instance().getSemanticMaxSegmentMs() << "ms";
    qDebug() << "[Summary] 音频权重:" << ConfigManager::instance().getSemanticAudioWeight();
    qDebug() << "[Summary] 视频权重:" << ConfigManager::instance().getSemanticVideoWeight();

    if (useSemanticSeg && !m_asrResults.isEmpty()) {
        setState(SummaryState::ClassifyingScenes);
        emit progressDetailChanged(progress());
        qDebug() << "[Summary] 阶段: VLM 批量场景分类";
        classifyVideoScenes();
        qDebug() << "[Summary] 场景分类完成, 获取到" << m_sceneTags.size() << "个场景标签";
        if (m_stopRequested) return;

        setState(SummaryState::DetectingSemanticBoundaries);
        emit progressDetailChanged(progress());
        qDebug() << "[Summary] 阶段: 语义边界检测与分段";
        runSemanticSegmentation();
        qDebug() << "[Summary] 语义分段完成, 共" << m_segments.size() << "段";
    } else {
        if (m_asrResults.isEmpty()) {
            qDebug() << "[Summary] 无语音数据，降级为固定时长分段";
        } else {
            qDebug() << "[Summary] 语义分段未启用，使用固定时长分段";
        }
        int segDuration = ConfigManager::instance().getSummarySegmentDuration();
        m_segments = SummarySegmenter::segmentByDuration(m_durationMs, segDuration);
        qDebug() << "[Summary] 固定分段完成, 共" << m_segments.size() << "段, 每段" << segDuration << "ms";
    }

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

    // 落盘缓存 (默认开启;失败不致命)
    saveToCache(m_videoPath);

    cleanupTempFiles();

    setState(SummaryState::Finished);
    emit progressDetailChanged(progress());
    qDebug() << "[Summary] 分析全部完成";
}

void VideoSummaryManager::extractFrames() {
    const int FRAME_INTERVAL_MS = 2500;
    int totalFramesNeeded = qMax(1, int(m_durationMs / FRAME_INTERVAL_MS));

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

// ===== 分析结果缓存 =====

static QJsonObject subtitleItemToJson(const SubtitleItem& s) {
    QJsonObject o;
    o["start"] = s.start_sec;
    o["end"] = s.end_sec;
    o["text"] = QString::fromStdString(s.text);
    return o;
}

static SubtitleItem subtitleItemFromJson(const QJsonObject& o) {
    SubtitleItem s;
    s.start_sec = o["start"].toDouble();
    s.end_sec = o["end"].toDouble();
    s.text = o["text"].toString().toStdString();
    return s;
}

static QJsonObject segmentToJson(const SummarySegment& s) {
    QJsonObject o;
    o["index"] = s.index;
    o["startMs"] = s.startMs;
    o["endMs"] = s.endMs;
    o["speechText"] = s.speechText;
    o["frameDescriptions"] = QJsonArray::fromStringList(s.frameDescriptions);
    o["description"] = s.description;
    o["isAnalyzed"] = s.isAnalyzed;
    return o;
}

static SummarySegment segmentFromJson(const QJsonObject& o) {
    SummarySegment s;
    s.index = o["index"].toInt();
    s.startMs = (qint64)o["startMs"].toDouble();
    s.endMs = (qint64)o["endMs"].toDouble();
    s.speechText = o["speechText"].toString();
    s.frameDescriptions = o["frameDescriptions"].toVariant().toStringList();
    s.description = o["description"].toString();
    s.isAnalyzed = o["isAnalyzed"].toBool();
    return s;
}

bool VideoSummaryManager::tryLoadFromCache(const QString& videoPath) {
    if (!ConfigManager::instance().getSummaryCacheEnabled()) return false;
    QString key = ConfigManager::instance().computeVideoCacheKey(videoPath);
    if (key.isEmpty()) return false;

    QString cacheFile = ConfigManager::instance().getSummaryCacheDir()
                        + "/" + key + ".json";
    QFile f(cacheFile);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[SummaryCache] JSON 解析失败,丢弃缓存:" << cacheFile << err.errorString();
        QFile::remove(cacheFile);
        return false;
    }

    QJsonObject root = doc.object();
    if (root["cacheKey"].toString() != key) {
        qWarning() << "[SummaryCache] cacheKey 不匹配,丢弃:" << cacheFile;
        QFile::remove(cacheFile);
        return false;
    }

    QJsonObject r = root["report"].toObject();

    // 用 m_fullReport 暂存,populateFromReport 会从 manager->report() 读
    m_fullReport = SummaryReport{};
    m_fullReport.tldr = r["tldr"].toString();
    m_fullReport.fullMarkdown = r["fullMarkdown"].toString();
    m_fullReport.videoDurationMs = (qint64)r["videoDurationMs"].toDouble();
    m_fullReport.generatedAt = QDateTime::fromString(r["generatedAt"].toString(), Qt::ISODate);

    QJsonArray ktArr = r["keyTakeaways"].toArray();
    for (const QJsonValue& v : ktArr) m_fullReport.keyTakeaways.append(v.toString());

    QJsonArray enArr = r["entities"].toArray();
    for (const QJsonValue& v : enArr) {
        QJsonObject e = v.toObject();
        SummaryEntity ent;
        ent.name = e["name"].toString();
        ent.type = e["type"].toString();
        ent.firstMentionMs = (qint64)e["firstMentionMs"].toDouble();
        m_fullReport.entities.append(ent);
    }

    QJsonArray chArr = r["chapters"].toArray();
    for (const QJsonValue& v : chArr) {
        QJsonObject c = v.toObject();
        SummaryChapter ch;
        ch.startMs = (qint64)c["startMs"].toDouble();
        ch.endMs = (qint64)c["endMs"].toDouble();
        ch.title = c["title"].toString();
        m_fullReport.chapters.append(ch);
    }

    QJsonArray segArr = r["segments"].toArray();
    for (const QJsonValue& v : segArr) m_fullReport.segments.append(segmentFromJson(v.toObject()));

    QJsonArray asrArr = r["asrResults"].toArray();
    for (const QJsonValue& v : asrArr) m_fullReport.asrResults.append(subtitleItemFromJson(v.toObject()));

    m_fullReport.isValid = true;

    // 同步到 m_segments / m_asrResults (用于章节列表高亮等)
    m_segments = m_fullReport.segments;
    m_asrResults = m_fullReport.asrResults;
    m_durationMs = m_fullReport.videoDurationMs;

    qDebug() << "[SummaryCache] 命中:" << cacheFile
             << "  (" << m_fullReport.chapters.size() << "章,"
             << m_fullReport.segments.size() << "段)";
    return true;
}

void VideoSummaryManager::saveToCache(const QString& videoPath) {
    if (!ConfigManager::instance().getSummaryCacheEnabled()) return;
    if (!m_fullReport.isValid) return;

    QString key = ConfigManager::instance().computeVideoCacheKey(videoPath);
    if (key.isEmpty()) return;

    // 把当前 m_segments / m_asrResults 也塞进 report (确保二次播放能还原章节/字幕)
    m_fullReport.segments = m_segments;
    m_fullReport.asrResults = m_asrResults;
    m_fullReport.videoDurationMs = m_durationMs;
    m_fullReport.generatedAt = QDateTime::currentDateTime();

    QJsonObject r;
    r["tldr"] = m_fullReport.tldr;
    r["fullMarkdown"] = m_fullReport.fullMarkdown;
    r["videoDurationMs"] = (double)m_fullReport.videoDurationMs;
    r["generatedAt"] = m_fullReport.generatedAt.toString(Qt::ISODate);

    QJsonArray ktArr;
    for (const QString& s : m_fullReport.keyTakeaways) ktArr.append(s);
    r["keyTakeaways"] = ktArr;

    QJsonArray enArr;
    for (const SummaryEntity& e : m_fullReport.entities) {
        QJsonObject o;
        o["name"] = e.name;
        o["type"] = e.type;
        o["firstMentionMs"] = (double)e.firstMentionMs;
        enArr.append(o);
    }
    r["entities"] = enArr;

    QJsonArray chArr;
    for (const SummaryChapter& c : m_fullReport.chapters) {
        QJsonObject o;
        o["startMs"] = (double)c.startMs;
        o["endMs"] = (double)c.endMs;
        o["title"] = c.title;
        chArr.append(o);
    }
    r["chapters"] = chArr;

    QJsonArray segArr;
    for (const SummarySegment& s : m_fullReport.segments) segArr.append(segmentToJson(s));
    r["segments"] = segArr;

    QJsonArray asrArr;
    for (const SubtitleItem& s : m_fullReport.asrResults) asrArr.append(subtitleItemToJson(s));
    r["asrResults"] = asrArr;

    QJsonObject root;
    root["cacheKey"] = key;
    root["videoPath"] = videoPath;
    root["schemaVersion"] = 1;
    root["report"] = r;

    QString cacheFile = ConfigManager::instance().getSummaryCacheDir()
                        + "/" + key + ".json";
    QFile f(cacheFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
        qDebug() << "[SummaryCache] 已保存:" << cacheFile;
    } else {
        qWarning() << "[SummaryCache] 写入失败:" << cacheFile << f.errorString();
    }
}

void VideoSummaryManager::clearAllCache() {
    QString dir = ConfigManager::instance().getSummaryCacheDir();
    QDir d(dir);
    int n = 0;
    for (const QString& f : d.entryList({"*.json"}, QDir::Files)) {
        if (QFile::remove(d.absoluteFilePath(f))) ++n;
    }
    qDebug() << "[SummaryCache] 清空完成,删除" << n << "个文件";
}

qint64 VideoSummaryManager::cacheTotalSize() {
    QString dir = ConfigManager::instance().getSummaryCacheDir();
    QDir d(dir);
    qint64 total = 0;
    for (const QFileInfo& fi : d.entryInfoList({"*.json"}, QDir::Files)) {
        total += fi.size();
    }
    return total;
}

int VideoSummaryManager::cacheFileCount() {
    QString dir = ConfigManager::instance().getSummaryCacheDir();
    return QDir(dir).entryList({"*.json"}, QDir::Files).size();
}
