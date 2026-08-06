#include "tencenttranslator.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDebug>

TencentTranslator::TencentTranslator() = default;
TencentTranslator::~TencentTranslator() { release(); }

bool TencentTranslator::init(const TranslateConfig& cfg) {
    cfg_ = cfg;

    // 从环境变量读取密钥
    const char* id_env = std::getenv("TENCENT_SECRET_ID");
    const char* key_env = std::getenv("TENCENT_SECRET_KEY");

    if (id_env) secret_id_ = id_env;
    if (key_env) secret_key_ = key_env;

    if (secret_id_.empty() || secret_key_.empty()) {
        qDebug() << "[TencentTranslator] missing TENCENT_SECRET_ID or TENCENT_SECRET_KEY";
        return false;
    }

    if (cfg_.api_endpoint.empty()) {
        cfg_.api_endpoint = "https://tmt.tencentcloudapi.com";
    }

    network_ = std::make_unique<QNetworkAccessManager>();
    ready_ = true;
    return true;
}

void TencentTranslator::release() {
    network_.reset();
    ready_ = false;
}

TranslateResult TencentTranslator::translate(const std::string& text) {
    TranslateResult result;
    result.source_text = text;

    if (!ready_ || text.empty()) {
        result.error_msg = "not ready or empty input";
        return result;
    }

    std::vector<std::string> texts = {text};
    std::string response = callApi(texts);

    if (response.empty()) {
        result.error_msg = "API call failed";
        return result;
    }

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response));
    QJsonObject resp = doc.object()["Response"].toObject();
    QString target_text = resp["TargetText"].toString().trimmed();

    if (!target_text.isEmpty()) {
        result.translated_text = target_text.toStdString();
        result.success = true;
    } else {
        result.error_msg = "empty translation result";
    }

    return result;
}

std::vector<TranslateResult> TencentTranslator::translateBatch(const std::vector<std::string>& texts) {
    std::vector<TranslateResult> results;
    if (!ready_ || texts.empty()) return results;

    //腾讯云 TMT 支持 TextTranslateBatch，一次最多2000字符
    // 这里简单逐条翻译，生产环境应用 batch 接口
    for (const auto& text : texts) {
        results.push_back(translate(text));
    }

    return results;
}

std::string TencentTranslator::callApi(const std::vector<std::string>& texts) {
    if (!network_ || texts.empty()) return "";

    //构造 TextTranslate 请求体
    QJsonObject body;
    body["SourceText"] = QString::fromStdString(texts[0]);
    body["Source"] = QString::fromStdString(cfg_.source_lang);
    body["Target"] = QString::fromStdString(cfg_.target_lang);
    body["ProjectId"] = 0;

    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QString timestamp = QString::number(QDateTime::currentSecsSinceEpoch());

    // 签名（TC3-HMAC-SHA256）
    std::string signature = generateSignature(payload.toStdString(), timestamp.toStdString());

    QNetworkRequest request(QUrl(QString::fromStdString(cfg_.api_endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-TC-Action", "TextTranslate");
    request.setRawHeader("X-TC-Version", "2018-03-21");
    request.setRawHeader("X-TC-Timestamp", timestamp.toUtf8());
    request.setRawHeader("Authorization", QByteArray::fromStdString(signature));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QNetworkReply* reply = network_->post(request, payload);
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
        result = reply->readAll().toStdString();
    } else {
        qDebug() << "[TencentTranslator] API error:" << reply->errorString();
    }

    reply->deleteLater();
    return result;
}

std::string TencentTranslator::generateSignature(const std::string& payload,
                                                  const std::string& timestamp) const {
    // TC3-HMAC-SHA256 签名算法
    // 参考：https://cloud.tencent.com/document/api/551/30636
    QString date = QDateTime::fromSecsSinceEpoch(timestamp.toInt()).toUTC().toString("yyyy-MM-dd");
    QString service = "tmt";

    // 1. 拼接规范请求串
    QString canonical_request = QString("POST\n/\n\ncontent-type:application/json\nhost:tmt.tencentcloudapi.com\n\ncontent-type;host\n") +
        QCryptographicHash::hash(QByteArray::fromStdString(payload), QCryptographicHash::Sha256).toHex();

    // 2. 拼接待签名字符串
    QString credential_scope = date + "/" + service + "/tc3_request";
    QString string_to_sign = "TC3-HMAC-SHA256\n" + QString::fromStdString(timestamp) + "\n" +
        credential_scope + "\n" +
        QCryptographicHash::hash(canonical_request.toUtf8(), QCryptographicHash::Sha256).toHex();

    // 3. 计算签名
    auto hmacSha256 = [](const QByteArray& key, const QByteArray& data) -> QByteArray {
        return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
    };

    QByteArray secret_date = hmacSha256("TC3" + QByteArray::fromStdString(secret_key_), date.toUtf8());
    QByteArray secret_service = hmacSha256(secret_date, service.toUtf8());
    QByteArray secret_signing = hmacSha256(secret_service, "tc3_request");
    QByteArray sign = hmacSha256(secret_signing, string_to_sign.toUtf8()).toHex();

    // 4. 拼接 Authorization
    QString auth = "TC3-HMAC-SHA256Credential=" + QString::fromStdString(secret_id_) +
        "/" + credential_scope + ", SignedHeaders=content-type;host, Signature=" + sign;

    return auth.toStdString();
}
