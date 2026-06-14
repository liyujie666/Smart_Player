#ifndef VIDEOSUMMARYNETWORKCLIENT_H
#define VIDEOSUMMARYNETWORKCLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QMutex>
#include <QSemaphore>
#include <QList>
#include <QJsonObject>
#include "queue/subtitlequeue.h"

struct SummarySegment {
    int index = 0;
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString speechText;
    QStringList frameDescriptions;
    QString description;
    bool isAnalyzed = false;
};

struct SummaryEntity {
    QString name;
    QString type;       // "concept", "person", "term", etc.
    qint64 firstMentionMs = 0;
};

struct SummaryChapter {
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString title;
};

struct SummaryReport {
    QString tldr;
    QStringList keyTakeaways;
    QList<SummaryEntity> entities;
    QList<SummaryChapter> chapters;
    QString fullMarkdown;
    bool isValid = false;
};

class SummaryNetworkClient : public QObject {
    Q_OBJECT
public:
    explicit SummaryNetworkClient(QObject* parent = nullptr);
    ~SummaryNetworkClient();

    void setApiKey(const QString& key);
    void setBaseUrl(const QString& url);
    void setModel(const QString& model);
    void abortAll();

    QNetworkAccessManager* ensureNetworkManager();

    void analyzeFrame(int segmentIndex,
                      const QByteArray& jpegData,
                      const QString& speechContext,
                      const QString& timeRange);
    void generateFullReportOnNetworkThread(const QList<SummarySegment>& segments,
                                          const QList<SubtitleItem>& asrResults);

signals:
    void frameAnalyzed(int segmentIndex, const QString& description, bool hasError, const QString& errorMsg);
    void reportReady(const QString& reportJson, bool hasError, const QString& errorMsg);
    void sceneClassified(qint64 timestampMs, const QString& sceneTag, bool hasError, const QString& errorMsg);

public slots:
    void classifySingleScene(qint64 timestampMs, const QByteArray& jpegData);

private:
    void postJson(const QString& endpoint,
                  const QJsonObject& body,
                  std::function<void(const QJsonObject&)> onDone,
                  std::function<void(const QString&)> onError);
    SummaryReport parseReportJson(const QJsonObject& obj) const;
    QString buildSegmentContext(const QList<SummarySegment>& segments,
                               const QList<SubtitleItem>& asrResults) const;

    QNetworkAccessManager* m_nam = nullptr;
    QString m_apiKey;
    QString m_baseUrl = QStringLiteral(u"https://dashscope.aliyuncs.com/compatible-mode/v1");
    QString m_model = QStringLiteral(u"qwen-vl-plus");
    QList<QNetworkReply*> m_pendingReplies;
    QSemaphore m_concurrencyLimit{5};
};

class SummaryNetworkBridge : public QObject {
    Q_OBJECT
public:
    explicit SummaryNetworkBridge(SummaryNetworkClient* client, QObject* parent = nullptr)
        : QObject(parent), m_client(client) {}

public slots:
    void analyzeFrame(int segmentIndex, const QByteArray& jpegData,
                      const QString& speechText, const QString& timeRange) {
        m_client->analyzeFrame(segmentIndex, jpegData, speechText, timeRange);
    }
    void generateFullReport(const QList<SummarySegment>& segments,
                           const QList<SubtitleItem>& asrResults) {
        m_client->generateFullReportOnNetworkThread(segments, asrResults);
    }
    void classifySingleScene(qint64 timestampMs, const QByteArray& jpegData) {
        m_client->classifySingleScene(timestampMs, jpegData);
    }

private:
    SummaryNetworkClient* m_client = nullptr;
};

#endif // VIDEOSUMMARYNETWORKCLIENT_H
