#include "cloudasrengine.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QByteArray>
#include <cstring>

CloudAsrEngine::CloudAsrEngine() = default;
CloudAsrEngine::~CloudAsrEngine() { release(); }

bool CloudAsrEngine::init(const AsrEngineConfig& cfg) {
    cfg_ = cfg;

    if (cloud_cfg_.api_key.empty()) {
        const char* env_key = std::getenv("SMARTPLAYER_CLOUD_ASR_KEY");
        if (env_key) {
            cloud_cfg_.api_key = env_key;
        } else {
            qDebug() << "[CloudAsrEngine] no API key provided";
            return false;
        }
    }

    if (cloud_cfg_.api_endpoint.empty()) {
        // 默认使用腾讯云一句话识别
        cloud_cfg_.api_endpoint = "https://asr.tencentcloudapi.com";
    }

    network_ = std::make_unique<QNetworkAccessManager>();
    ready_ = true;
    return true;
}

void CloudAsrEngine::release() {
    network_.reset();
    ready_ = false;
}

bool CloudAsrEngine::recognize(const std::vector<float>& pcm,
                                std::vector<SubtitleItem>& out,
                                double base_sec) {
    if (!ready_ || pcm.empty()) return false;
    out.clear();

    std::string audio_data = encodePcm(pcm);
    std::string response = callCloudApi(audio_data);

    if (response.empty()) return false;
    return parseResponse(response, out, base_sec);
}

void CloudAsrEngine::reset() {
    // 云端引擎无状态，无需重置
}

std::string CloudAsrEngine::encodePcm(const std::vector<float>& pcm) const {
    // 将 float32 PCM 转换为 16bit PCM 再base64 编码
    std::vector<int16_t> pcm16(pcm.size());
    for (size_t i = 0; i < pcm.size(); ++i) {
        float s = pcm[i] * 32767.0f;
        if (s > 32767.0f) s = 32767.0f;
        if (s < -32768.0f) s = -32768.0f;
        pcm16[i] = (int16_t)s;
    }

    QByteArray raw(reinterpret_cast<const char*>(pcm16.data()), pcm16.size() * sizeof(int16_t));
    return raw.toBase64().toStdString();
}

std::string CloudAsrEngine::callCloudApi(const std::string& audio_data) {
    if (!network_) return "";

    // TODO: 根据 cloud_cfg_.provider 构造不同的请求格式
    // 这里以腾讯云一句话识别为例

    QJsonObject body;
    body["ProjectId"] = 0;
    body["SubServiceType"] = 2;
    body["EngSerViceType"] = "16k_zh";
    body["SourceType"] = 1;
    body["Data"] = QString::fromStdString(audio_data);
    body["DataLen"] = (int)(audio_data.size());
    body["VoiceFormat"] = "pcm";

    QNetworkRequest request(QUrl(QString::fromStdString(cloud_cfg_.api_endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // TODO: 腾讯云签名认证
    request.setRawHeader("Authorization", QByteArray::fromStdString(cloud_cfg_.api_key));

    QByteArray data = QJsonDocument(body).toJson();

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QNetworkReply* reply = network_->post(request, data);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(cloud_cfg_.timeout_ms);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return "";
    }

    std::string result;
    if (reply->error() == QNetworkReply::NoError) {
        result = reply->readAll().toStdString();
    } else {
        qDebug() << "[CloudAsrEngine] API error:" << reply->errorString();
    }

    reply->deleteLater();
    return result;
}

bool CloudAsrEngine::parseResponse(const std::string& response,
                                    std::vector<SubtitleItem>& out,
                                    double base_sec) {
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response));
    if (doc.isNull()) return false;

    QJsonObject obj = doc.object();

    // TODO: 根据 provider 解析不同格式的响应
    // 腾讯云格式：Response.Result
    QJsonObject resp = obj["Response"].toObject();
    QString text = resp["Result"].toString().trimmed();

    if (text.isEmpty()) return false;

    SubtitleItem item;
    item.text = text.toStdString();
    item.start_sec = base_sec;
    //估算结束时间（云端一句话识别通常不返回精确时间戳）
    item.end_sec = base_sec + 30.0;  // 按送入的音频长度估算
    out.push_back(item);

    return true;
}
