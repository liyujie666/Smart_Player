#include "videosummarynetworkclient.h"
#include "queue/subtitlequeue.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QDebug>

SummaryNetworkClient::SummaryNetworkClient(QObject* parent)
    : QObject(parent)
{
}

QNetworkAccessManager* SummaryNetworkClient::ensureNetworkManager() {
    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
    }
    return m_nam;
}

SummaryNetworkClient::~SummaryNetworkClient() {
    abortAll();
}

void SummaryNetworkClient::setApiKey(const QString& key) {
    m_apiKey = key;
}

void SummaryNetworkClient::setBaseUrl(const QString& url) {
    m_baseUrl = url;
}

void SummaryNetworkClient::setModel(const QString& model) {
    m_model = model;
}

void SummaryNetworkClient::abortAll() {
    for (QNetworkReply* reply : std::as_const(m_pendingReplies)) {
        if (reply) reply->abort();
    }
    m_pendingReplies.clear();
}

void SummaryNetworkClient::postJson(const QString& endpoint,
                                    const QJsonObject& body,
                                    std::function<void(const QJsonObject&)> onDone,
                                    std::function<void(const QString&)> onError) {
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());

    QNetworkReply* reply = ensureNetworkManager()->post(req, QJsonDocument(body).toJson());
    m_pendingReplies.append(reply);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        m_pendingReplies.removeAll(reply);
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            // abortAll 主动取消:不调 onDone (resp 为空会越界),不调 onError
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            onError(reply->errorString());
        } else {
            QJsonObject resp = QJsonDocument::fromJson(reply->readAll()).object();
            if (resp.contains("error")) {
                onError(resp["error"].toObject()["message"].toString());
            } else {
                onDone(resp);
            }
        }
        reply->deleteLater();
    });
}

SummaryReport SummaryNetworkClient::parseReportJson(const QJsonObject& obj) const {
    SummaryReport report;
    report.tldr = obj["tldr"].toString();
    QJsonArray takeaways = obj["key_takeaways"].toArray();
    for (const QJsonValue& v : takeaways) {
        report.keyTakeaways.append(v.toString());
    }
    QJsonArray entities = obj["entities"].toArray();
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
        report.entities.append(e);
    }
    QJsonArray chapters = obj["chapters"].toArray();
    for (const QJsonValue& v : chapters) {
        SummaryChapter ch;
        QJsonObject c = v.toObject();
        QString startStr = c["start"].toString();
        QString endStr = c["end"].toString();
        ch.title = c["title"].toString();

        auto parseTs = [](const QString& s) -> qint64 {
            QStringList parts = s.split(':');
            if (parts.size() == 2) {
                int mins = parts[0].toInt();
                int secs = qRound(parts[1].toDouble());
                return (mins * 60 + secs) * 1000;
            }
            return 0LL;
        };
        ch.startMs = parseTs(startStr);
        ch.endMs = parseTs(endStr);
        report.chapters.append(ch);
    }
    report.fullMarkdown = obj["markdown"].toString();
    report.isValid = true;
    return report;
}

QString SummaryNetworkClient::buildSegmentContext(const QList<SummarySegment>& segments,
                                                  const QList<SubtitleItem>& asrResults) const {
    QStringList lines;
    for (const auto& seg : segments) {
        if (!seg.isAnalyzed || seg.description.isEmpty()) continue;
        if (seg.description.contains(QStringLiteral(u"(无画面"))) continue;

        QString ts = QStringLiteral(u"[%1-%2s]")
            .arg(seg.startMs / 1000.0, 0, 'f', 1)
            .arg(seg.endMs / 1000.0, 0, 'f', 1);

        QString speech;
        for (const auto& sub : asrResults) {
            double endSec = sub.end_sec;
            double startSec = sub.start_sec;
            if (endSec * 1000 > seg.startMs && startSec * 1000 < seg.endMs) {
                if (!speech.isEmpty()) speech += " ";
                speech += QString::fromStdString(sub.text);
            }
        }

        QString line = ts + QStringLiteral(u" | 画面: %1").arg(seg.description);
        if (!speech.isEmpty()) {
            line += QStringLiteral(u" | 语音: %1").arg(speech);
        }
        lines.append(line);
    }
    return lines.join("\n");
}

void SummaryNetworkClient::analyzeFrame(int segmentIndex,
                                       const QByteArray& jpegData,
                                       const QString& speechContext,
                                       const QString& timeRange) {
    QString prompt;
    if (!speechContext.isEmpty()) {
        prompt = QStringLiteral(
            u"【时间范围】%1\n"
            u"【该时段的语音内容】\"%2\"\n\n"
            u"请同时结合画面和语音，描述这个时间段发生了什么。"
            u"注意人物的话语/旁白和画面动作的配合关系。"
            u"如果画面中有文字，请转录出来。\n\n"
            u"请用2-3句话描述。"
        ).arg(timeRange).arg(speechContext);
    } else {
        prompt = QStringLiteral(
            u"【时间范围】%1\n"
            u"请描述该时间段的画面内容，包括人物动作、场景变化、文字等细节。"
            u"该时段无语音，为纯画面内容。"
        ).arg(timeRange);
    }

    QString imageBase64 = QString::fromLatin1(jpegData.toBase64());

    QJsonObject userContent;
    userContent["role"] = "user";
    QJsonArray contentArray;
    contentArray.append(QJsonObject{
        {"type", "text"},
        {"text", prompt}
    });
    contentArray.append(QJsonObject{
        {"type", "image_url"},
        {"image_url", QJsonObject{
            {"url", "data:image/jpeg;base64," + imageBase64}
        }}
    });
    userContent["content"] = contentArray;

    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = QJsonArray({userContent});

    postJson("/chat/completions", body,
        [this, segmentIndex](const QJsonObject& resp) {
            QString content = resp["choices"].toArray()[0]
                .toObject()["message"].toObject()
                ["content"].toString();
            emit frameAnalyzed(segmentIndex, content, false, QString());
        },
        [this, segmentIndex](const QString& err) {
            emit frameAnalyzed(segmentIndex, QString(), true, err);
        });
}

void SummaryNetworkClient::generateFullReportOnNetworkThread(const QList<SummarySegment>& segments,
                                                            const QList<SubtitleItem>& asrResults) {
    QString allContent = buildSegmentContext(segments, asrResults);

    if (allContent.isEmpty()) {
        emit reportReady(QString(), true, QStringLiteral(u"无有效分析结果可生成报告。"));
        return;
    }

    QString systemPrompt = QStringLiteral(
        u"你是一个专业的视频内容分析助手。请根据视频各时间段的描述，生成一份结构化的视频总结报告。\n\n"
        u"【重要】你必须且只能返回一个有效的 JSON 对象，不要包含任何其他文字，JSON 格式如下：\n"
        u"{\n"
        u"  \"tldr\": \"一句话总结，不超过60字，概括视频最核心的内容\",\n"
        u"  \"key_takeaways\": [\"要点1\", \"要点2\", \"要点3\"],\n"
        u"  \"entities\": [\n"
        u"    {\"name\": \"实体名称\", \"type\": \"concept|person|term|event\", \"first_mention\": \"MM:SS\"},\n"
        u"    ...\n"
        u"  ],\n"
        u"  \"chapters\": [\n"
        u"    {\"start\": \"MM:SS\", \"end\": \"MM:SS\", \"title\": \"章节标题\"},\n"
        u"    ...\n"
        u"  ],\n"
        u"  \"markdown\": \"完整的 Markdown 格式报告，包含 TL;DR、关键要点、章节列表和详细内容\"\n"
        u"}\n\n"
        u"【硬性约束 - 章节时间戳】\n"
        u"- chapters 的个数必须等于下方提供的\"时间段\"个数 (下面会列出 N 个段，你必须输出 N 个 chapter)\n"
        u"- 每个 chapter 的 start/end 必须与对应时间段的起止时间戳完全一致 (MM:SS 格式)\n"
        u"- 不允许把一个时间段拆成多个 chapter，也不允许把多个时间段合并成一个\n"
        u"- 章节标题(title)要简短精炼 (10-20 字)，概括该时间段的核心内容\n\n"
        u"其他要求：\n"
        u"- tldr 不超过60字\n"
        u"- key_takeaways 提取3-5条最重要的观点\n"
        u"- entities 最多提取10个重要概念/人物，first_mention 格式为 MM:SS（首次出现时间）\n"
        u"- markdown 是一份完整可读的 Markdown 报告"
    );

    QString userPrompt = QStringLiteral(
        u"以下是一个视频的各时间段分析结果：\n\n%1\n\n"
        u"请根据以上内容生成结构化的视频总结报告（仅返回 JSON，不要有其他文字）。"
    ).arg(allContent);

    QJsonObject systemContent;
    systemContent["role"] = "system";
    systemContent["content"] = systemPrompt;

    QJsonObject userContent;
    userContent["role"] = "user";
    userContent["content"] = userPrompt;

    QJsonObject body;
    body["model"] = "qwen-plus";
    body["messages"] = QJsonArray({systemContent, userContent});

    postJson("/chat/completions", body,
        [this, segments](const QJsonObject& resp) {
            QString rawContent = resp["choices"].toArray()[0]
                .toObject()["message"].toObject()
                ["content"].toString();

            rawContent = rawContent.trimmed();
            if (rawContent.startsWith("```json")) {
                rawContent = rawContent.mid(7);
            }
            if (rawContent.startsWith("```")) {
                rawContent = rawContent.mid(3);
            }
            if (rawContent.endsWith("```")) {
                rawContent = rawContent.left(rawContent.size() - 3);
            }
            rawContent = rawContent.trimmed();

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(rawContent.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError) {
                emit reportReady(QString(), true,
                    QStringLiteral(u"报告解析失败: %1").arg(err.errorString()));
                return;
            }

            QJsonObject obj = doc.object();
            if (!obj.contains("tldr") && !obj.contains("key_takeaways")) {
                emit reportReady(QString(), true,
                    QStringLiteral(u"报告格式错误：缺少必要字段"));
                return;
            }

            {
                QJsonArray llmChapters = obj["chapters"].toArray();
                QJsonArray segArr;
                for (const auto& seg : segments) {
                    QJsonObject s;
                    s["start"] = QStringLiteral(u"%1:%2")
                        .arg(seg.startMs / 60000, 2, 10, QChar('0'))
                        .arg((seg.startMs % 60000) / 1000, 2, 10, QChar('0'));
                    s["end"] = QStringLiteral(u"%1:%2")
                        .arg(seg.endMs / 60000, 2, 10, QChar('0'))
                        .arg((seg.endMs % 60000) / 1000, 2, 10, QChar('0'));
                    s["title"] = QString();
                    segArr.append(s);
                }

                auto copyTitleIfAny = [&](int i) {
                    if (i < 0 || i >= segArr.size() || i >= llmChapters.size()) return;
                    QJsonObject segObj = segArr[i].toObject();
                    QString title = llmChapters[i].toObject()["title"].toString();
                    if (!title.isEmpty()) {
                        segObj["title"] = title;
                    }
                    segArr.replace(i, segObj);
                };

                if (llmChapters.size() != segments.size()) {
                    qWarning() << "[Summary] LLM 章节数不匹配 segments (LLM:"
                               << llmChapters.size() << ", segments:" << segments.size()
                               << "), 强制使用 segment 时间戳";
                }
                for (int i = 0; i < segArr.size(); ++i) copyTitleIfAny(i);
                obj["chapters"] = segArr;

                {
                    QJsonArray sb;
                    for (int i = 0; i < segArr.size(); ++i) {
                        QJsonObject c = segArr[i].toObject();
                        QString line = QStringLiteral(u"- [%1 - %2] %3")
                            .arg(c["start"].toString(), c["end"].toString(),
                                 c["title"].toString().isEmpty()
                                    ? QStringLiteral(u"第 %1 段").arg(i + 1)
                                    : c["title"].toString());
                        sb.append(line);
                    }
                    obj["chapter_timeline"] = sb;
                }
            }

            SummaryReport report = parseReportJson(obj);
            QJsonDocument outDoc;
            outDoc.setObject(obj);
            emit reportReady(QString::fromUtf8(outDoc.toJson(QJsonDocument::Compact)), false, QString());
        },
        [this](const QString& err) {
            emit reportReady(QString(), true, err);
        });
}

void SummaryNetworkClient::classifySingleScene(qint64 timestampMs, const QByteArray& jpegData) {
    if (!m_concurrencyLimit.tryAcquire()) {
        QTimer::singleShot(100, this, [this, timestampMs, jpegData]() {
            classifySingleScene(timestampMs, jpegData);
        });
        return;
    }

    QString prompt = QStringLiteral(
        u"请用 1-3 个英文单词描述这个画面的场景类型，"
        u"如：outdoor_talking / code_demo / slide_presentation / "
        u"product_shot / crowd_scene / screen_recording / "
        u"driving_view / indoor_meeting / cooking_scene / "
        u"sports_action / music_performance。\n\n"
        u"只返回场景标签（如 outdoor_talking），不要其他任何内容。"
    );

    QString imageBase64 = QString::fromLatin1(jpegData.toBase64());

    QJsonObject userContent;
    userContent["role"] = "user";
    QJsonArray contentArray;
    contentArray.append(QJsonObject{
        {"type", "text"},
        {"text", prompt}
    });
    contentArray.append(QJsonObject{
        {"type", "image_url"},
        {"image_url", QJsonObject{
            {"url", "data:image/jpeg;base64," + imageBase64}
        }}
    });
    userContent["content"] = contentArray;

    QJsonObject body;
    body["model"] = m_model;
    body["messages"] = QJsonArray({userContent});

    postJson("/chat/completions", body,
        [this, timestampMs](const QJsonObject& resp) {
            QJsonArray choices = resp["choices"].toArray();
            if (choices.isEmpty()) {
                qWarning() << "[Summary] classifySingleScene: 空 choices,timestampMs=" << timestampMs;
                m_concurrencyLimit.release();
                emit sceneClassified(timestampMs, QString(), true,
                                     QStringLiteral("empty choices from API"));
                return;
            }
            QString content = choices[0].toObject()["message"].toObject()
                              ["content"].toString();
            m_concurrencyLimit.release();
            emit sceneClassified(timestampMs, content.trimmed(), false, QString());
        },
        [this, timestampMs](const QString& err) {
            m_concurrencyLimit.release();
            emit sceneClassified(timestampMs, QString(), true, err);
        });
}
