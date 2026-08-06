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
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QDebug>

TencentTranslator::TencentTranslator() = default;
TencentTranslator::~TencentTranslator() { release(); }

void TencentTranslator::loadConfigFromFile() {
    // 查找本地配置文件: config/tencent_translate.json
    // 路径优先级: 工作目录 → 用户配置目录
    QStringList search_paths = {
        QDir::currentPath() + "/config/tencent_translate.json",
        QDir::currentPath() + "/tencent_translate.json",
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/tencent_translate.json",
    };

    for (const QString& path : search_paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();

        if (secret_id_.empty()) {
            secret_id_ = obj["secret_id"].toString().toStdString();
        }
        if (secret_key_.empty()) {
            secret_key_ = obj["secret_key"].toString().toStdString();
        }

        if (!secret_id_.empty() && !secret_key_.empty()) {
            qDebug() << "[TencentTranslator] config loaded from" << path;
            return;
        }
    }
}

bool TencentTranslator::init(const TranslateConfig& cfg) {
    cfg_ = cfg;

    // 优先级: 1. 代码传入的 api_key → 2. 环境变量 → 3. 本地配置文件

    // 如果通过 TranslateConfig 传入了 secret_id 和 secret_key（用 "|" 分隔）
    if (!cfg_.api_key.empty()) {
        // api_key 格式: "secret_id|secret_key"
        size_t sep = cfg_.api_key.find('|');
        if (sep != std::string::npos) {
            secret_id_ = cfg_.api_key.substr(0, sep);
            secret_key_ = cfg_.api_key.substr(sep + 1);
        }
    }

    // 环境变量
    if (secret_id_.empty()) {
        const char* id_env = std::getenv("TENCENT_SECRET_ID");
        if (id_env) secret_id_ = id_env;
    }
    if (secret_key_.empty()) {
        const char* key_env = std::getenv("TENCENT_SECRET_KEY");
        if (key_env) secret_key_ = key_env;
    }

    // 本地配置文件
    if (secret_id_.empty() || secret_key_.empty()) {
        loadConfigFromFile();
    }

    if (secret_id_.empty() || secret_key_.empty()) {
        qDebug() << "[TencentTranslator] no API key found."
                 << "Set via TranslateConfig.api_key (\"id|key\"),"
                 << "env vars TENCENT_SECRET_ID/TENCENT_SECRET_KEY,"
                 << "or config/tencent_translate.json";
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

    // 检查 API 错误
    QJsonObject error = resp["Error"].toObject();
    if (!error.isEmpty()) {
        QString err_code = error["Code"].toString();
        QString err_msg = error["Message"].toString();
        result.error_msg = QString("%1: %2").arg(err_code, err_msg).toStdString();
        qDebug() << "[TencentTranslator] API error:" << err_code << err_msg;
        return result;
    }

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

    // 构造 TextTranslate 请求体
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
    request.setRawHeader("Host", "tmt.tencentcloudapi.com");
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

    // 4. 拼接 Authorization（注意：TC3-HMAC-SHA256 后有空格）
    QString auth = "TC3-HMAC-SHA256 Credential=" + QString::fromStdString(secret_id_) +
        "/" + credential_scope + ", SignedHeaders=content-type;host, Signature=" + sign;

    return auth.toStdString();
}
