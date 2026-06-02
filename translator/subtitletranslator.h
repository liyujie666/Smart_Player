#ifndef SUBTITLE_TRANSLATOR_H
#define SUBTITLE_TRANSLATOR_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QString>
#include <QCache>
#include <functional>
#include <memory>

// 翻译请求结构
struct TranslateRequest {
    int64_t id;           // 唯一ID，用于匹配结果
    QString sourceText;   // 原文
    QString sourceLang;   // 源语言 (如 "en", "zh", "ja")
    QString targetLang;   // 目标语言
    int64_t timestamp;    // 字幕时间戳(ms)，用于排序和丢弃过期请求
};

// 翻译结果结构
struct TranslateResult {
    int64_t id;
    QString sourceText;
    QString translatedText;
    int64_t timestamp;
    bool success;
};

// 翻译后端接口（策略模式，方便切换不同翻译引擎）
class ITranslateBackend {
public:
    virtual ~ITranslateBackend() = default;
    virtual bool initialize(const QString &modelPath) = 0;
    virtual QString translate(const QString &text, const QString &srcLang, const QString &tgtLang) = 0;
    virtual void release() = 0;
    virtual QString name() const = 0;
};

/**
 * @brief 字幕翻译器 - 异步非阻塞设计
 *
 * 设计要点：
 * 1. 独立工作线程，不阻塞音视频解码/渲染
 * 2. LRU缓存避免重复翻译
 * 3. 过期请求自动丢弃（当翻译速度跟不上字幕产生速度时）
 * 4. 支持热切换翻译引擎
 */
class SubtitleTranslator : public QObject
{
    Q_OBJECT

public:
    explicit SubtitleTranslator(QObject *parent = nullptr);
    ~SubtitleTranslator();

    // 初始化翻译引擎
    bool init(const QString &modelPath, const QString &backendType = "ctranslate2");

    // 设置语言对
    void setLanguagePair(const QString &srcLang, const QString &tgtLang);

    // 提交翻译请求（非阻塞）
    void submitTranslation(const QString &text, int64_t timestampMs);

    // 启动/停止
    void start();
    void stop();

    // 配置
    void setCacheSize(int maxEntries);
    void setMaxPendingRequests(int max);  // 队列满时丢弃最旧的请求
    void setExpireThresholdMs(int64_t ms); // 超过此阈值的请求视为过期

    // 状态
    bool isRunning() const;
    int pendingCount() const;

signals:
    // 翻译完成信号 - 连接到字幕渲染层
    void translationReady(const TranslateResult &result);

    // 错误信号
    void errorOccurred(const QString &error);

private:
    void workerLoop();
    void processRequest(const TranslateRequest &req);
    void discardExpiredRequests();

private:
    std::unique_ptr<ITranslateBackend> m_backend;
    std::unique_ptr<QThread> m_workerThread;

    // 请求队列（生产者-消费者模型）
    QQueue<TranslateRequest> m_requestQueue;
    mutable QMutex m_queueMutex;
    QWaitCondition m_queueCondition;

    // LRU翻译缓存
    QCache<QString, QString> m_cache;
    QMutex m_cacheMutex;

    // 配置
    QString m_srcLang;
    QString m_tgtLang;
    int m_maxPending = 50;
    int64_t m_expireThresholdMs = 3000; // 3秒过期

    // 状态
    std::atomic<bool> m_running{false};
    std::atomic<int64_t> m_nextId{0};
};

#endif // SUBTITLE_TRANSLATOR_H
