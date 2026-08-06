#include "gpttranslator.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>

GptTranslator::GptTranslator() = default;
GptTranslator::~GptTranslator() { release(); }

bool GptTranslator::init(const TranslateConfig& cfg) {
    cfg_ = cfg;

    if (cfg_.api_key.empty()) {
        //尝试从环境变量读取
        const char* env_key = std::getenv("SMARTPLAYER_TRANSLATE_API_KEY");
        if (env_key) {
            cfg_.api_key = env_key;
        } else {
            qDebug() << "[GptTranslator] no API key provided";
            return false;
        }
    }

    if (cfg_.api_endpoint.empty()) {
        cfg_.api_endpoint = "https://api.openai.com/v1/chat/completions";
    }

    network_ = std::make_unique<QNetworkAccessManager>();
    ready_ = true;
    return true;
}

void GptTranslator::release() {
    network_.reset();
    ready_ = false;
}

TranslateResult GptTranslator::translate(const std::string& text) {
    TranslateResult result;
    result.source_text = text;

    if (!ready_ || text.empty()) {
        result.error_msg = "translator not ready or empty input";
        return result;
    }

    std::string prompt = buildPrompt(text);
    std::string response = callApi(prompt);

    if (!response.empty()) {
        result.translated_text = response;
        result.success = true;
    } else {
        result.error_msg = "API call failed";
    }

    return result;
}

std::vector<TranslateResult> GptTranslator::translateBatch(const std::vector<std::string>& texts) {
    std::vector<TranslateResult> results;
    if (!ready_ || texts.empty()) return results;

    // 分批处理
    for (size_t i = 0; i < texts.size(); i += cfg_.max_batch_size) {
        size_t end = std::min(i + (size_t)cfg_.max_batch_size, texts.size());
        std::vector<std::string> batch(texts.begin() + i, texts.begin() + end);

        std::string prompt = buildBatchPrompt(batch);
        std::string response = callApi(prompt);

        if (!response.empty()) {
            //尝试按行分割结果
            std::vector<std::string> lines;
            std::string line;
            for (char c : response) {
                if (c == '\n') {
                    if (!line.empty()) lines.push_back(line);
                    line.clear();
                } else {
                    line += c;
                }
            }
            if (!line.empty()) lines.push_back(line);

            for (size_t j = 0; j < batch.size(); ++j) {
                TranslateResult r;
                r.source_text = batch[j];
                if (j < lines.size()) {
                    r.translated_text = lines[j];
                    r.success = true;
                } else {
                    r.error_msg = "batch result mismatch";
                }
                results.push_back(r);
            }
        } else {
            for (const auto& t : batch) {
                TranslateResult r;
                r.source_text = t;
                r.error_msg = "API call failed";
                results.push_back(r);
            }
        }
    }

    return results;
}

std::string GptTranslator::buildPrompt(const std::string& text) const {
    return "Translate the following text to " + cfg_.target_lang +
           ". Only output the translation, nothing else:\n" + text;
}

std::string GptTranslator::buildBatchPrompt(const std::vector<std::string>& texts) const {
    std::string prompt = "Translate each line to " + cfg_.target_lang +
                         ". Output one translation per line, maintaining the same order:\n";
    for (const auto& t : texts) {
        prompt += t + "\n";
    }
    return prompt;
}

std::string GptTranslator::callApi(const std::string& prompt) {
    if (!network_) return "";

    QJsonObject message;
    message["role"] = "user";
    message["content"] = QString::fromStdString(prompt);

    QJsonArray messages;
    messages.append(message);

    QJsonObject body;
    body["model"] = "gpt-4o-mini";
    body["messages"] = messages;
    body["temperature"] = 0.3;
    body["max_tokens"] = 2048;

    QNetworkRequest request(QUrl(QString::fromStdString(cfg_.api_endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + QByteArray::fromStdString(cfg_.api_key));

    QByteArray data = QJsonDocument(body).toJson();

    // 同步等待（在工作线程中调用，不阻塞 UI）
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QNetworkReply* reply = network_->post(request, data);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(cfg_.timeout_ms);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return "";
    }

    std::string result;
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();
        QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty()) {
            result = choices[0].toObject()["message"].toObject()["content"]
                         .toString().trimmed().toStdString();
        }
    } else {
        qDebug() << "[GptTranslator] API error:" << reply->errorString();
    }

    reply->deleteLater();
    return result;
}
