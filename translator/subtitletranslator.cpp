#include "subtitletranslator.h"
#include "ctranslate2backend.h"
#include <QDateTime>
#include <QDebug>

SubtitleTranslator::SubtitleTranslator(QObject *parent)
    : QObject(parent)
    , m_cache(200)  // 默认缓存200条翻译结果
{
}

SubtitleTranslator::~SubtitleTranslator()
{
    stop();
}

bool SubtitleTranslator::init(const QString &modelPath, const QString &backendType)
{
    // 根据类型创建翻译后端
    if (backendType == "ctranslate2") {
        m_backend = std::make_unique<CTranslate2Backend>();
    }
    // 可扩展其他后端: "libretranslate", "online_api" 等

    if (!m_backend) {
        emit errorOccurred("Unsupported backend type: " + backendType);
        return false;
    }

    if (!m_backend->initialize(modelPath)) {
        emit errorOccurred("Failed to initialize translation backend: " + m_backend->name());
        return false;
    }

    qDebug() << "[Translator] Initialized with backend:" << m_backend->name();
    return true;
}

void SubtitleTranslator::setLanguagePair(const QString &srcLang, const QString &tgtLang)
{
    m_srcLang = srcLang;
    m_tgtLang = tgtLang;
}

void SubtitleTranslator::submitTranslation(const QString &text, int64_t timestampMs)
{
    if (!m_running.load() || text.trimmed().isEmpty()) {
        return;
    }

    // 先查缓存
    QString cacheKey = m_srcLang + "_" + m_tgtLang + "_" + text;
    {
        QMutexLocker lock(&m_cacheMutex);
        if (QString *cached = m_cache.object(cacheKey)) {
            // 缓存命中，直接发送结果
            TranslateResult result;
            result.id = m_nextId.fetch_add(1);
            result.sourceText = text;
            result.translatedText = *cached;
            result.timestamp = timestampMs;
            result.success = true;
            emit translationReady(result);
            return;
        }
    }

    // 构建请求加入队列
    TranslateRequest req;
    req.id = m_nextId.fetch_add(1);
    req.sourceText = text;
    req.sourceLang = m_srcLang;
    req.targetLang = m_tgtLang;
    req.timestamp = timestampMs;

    {
        QMutexLocker lock(&m_queueMutex);

        // 队列过长时丢弃最旧的请求
        while (m_requestQueue.size() >= m_maxPending) {
            m_requestQueue.dequeue();
            qDebug() << "[Translator] Queue full, discarding oldest request";
        }

        m_requestQueue.enqueue(req);
        m_queueCondition.wakeOne();
    }
}

void SubtitleTranslator::start()
{
    if (m_running.load()) {
        return;
    }

    m_running.store(true);

    m_workerThread = std::make_unique<QThread>();
    QObject::connect(m_workerThread.get(), &QThread::started, [this]() {
        workerLoop();
    });
    m_workerThread->start();

    qDebug() << "[Translator] Worker thread started";
}

void SubtitleTranslator::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    // 唤醒工作线程使其退出
    m_queueCondition.wakeAll();

    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }

    // 清空队列
    QMutexLocker lock(&m_queueMutex);
    m_requestQueue.clear();

    qDebug() << "[Translator] Stopped";
}

void SubtitleTranslator::setCacheSize(int maxEntries)
{
    QMutexLocker lock(&m_cacheMutex);
    m_cache.setMaxCost(maxEntries);
}

void SubtitleTranslator::setMaxPendingRequests(int max)
{
    m_maxPending = max;
}

void SubtitleTranslator::setExpireThresholdMs(int64_t ms)
{
    m_expireThresholdMs = ms;
}

bool SubtitleTranslator::isRunning() const
{
    return m_running.load();
}

int SubtitleTranslator::pendingCount() const
{
    QMutexLocker lock(&m_queueMutex);
    return m_requestQueue.size();
}

void SubtitleTranslator::workerLoop()
{
    while (m_running.load()) {
        TranslateRequest req;

        {
            QMutexLocker lock(&m_queueMutex);

            // 等待请求
            while (m_requestQueue.isEmpty() && m_running.load()) {
                m_queueCondition.wait(&m_queueMutex, 100);
            }

            if (!m_running.load()) {
                break;
            }

            // 丢弃过期请求
            discardExpiredRequests();

            if (m_requestQueue.isEmpty()) {
                continue;
            }

            req = m_requestQueue.dequeue();
        }

        processRequest(req);
    }
}

void SubtitleTranslator::processRequest(const TranslateRequest &req)
{
    if (!m_backend) {
        return;
    }

    TranslateResult result;
    result.id = req.id;
    result.sourceText = req.sourceText;
    result.timestamp = req.timestamp;

    try {
        QString translated = m_backend->translate(req.sourceText, req.sourceLang, req.targetLang);

        result.translatedText = translated;
        result.success = !translated.isEmpty();

        // 写入缓存
        if (result.success) {
            QString cacheKey = req.sourceLang + "_" + req.targetLang + "_" + req.sourceText;
            QMutexLocker lock(&m_cacheMutex);
            m_cache.insert(cacheKey, new QString(translated));
        }
    } catch (const std::exception &e) {
        result.success = false;
        result.translatedText.clear();
        emit errorOccurred(QString("Translation error: %1").arg(e.what()));
    }

    emit translationReady(result);
}

void SubtitleTranslator::discardExpiredRequests()
{
    // 调用时已持有 m_queueMutex
    int64_t now = QDateTime::currentMSecsSinceEpoch();

    while (!m_requestQueue.isEmpty()) {
        const auto &front = m_requestQueue.head();
        if (now - front.timestamp > m_expireThresholdMs) {
            m_requestQueue.dequeue();
        } else {
            break;
        }
    }
}
