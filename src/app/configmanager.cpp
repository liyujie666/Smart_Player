#include "configmanager.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

ConfigManager::ConfigManager()
{
    QString appDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(appDir);

    settings_ = new QSettings(appDir + "/config.ini", QSettings::IniFormat, this);

    playlistFile_ = appDir + "/playlist.json";

    // Defaults
    if (!settings_->contains("hardware")) settings_->setValue("hardware", false);
    if (!settings_->contains("decoderFormat")) settings_->setValue("decoderFormat", "default");
    if (!settings_->contains("brightness")) settings_->setValue("brightness", 0);
    if (!settings_->contains("contrast")) settings_->setValue("contrast", 100);
    if (!settings_->contains("saturation")) settings_->setValue("saturation", 100);
    if (!settings_->contains("subtitleFontSize")) settings_->setValue("subtitleFontSize", 26);
    if (!settings_->contains("videoSizeMode")) settings_->setValue("videoSizeMode", 0);
    if (!settings_->contains("screenshotSavePath")) {
        settings_->setValue("screenshotSavePath", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    }
    if (!settings_->contains("modelPath")) settings_->setValue("modelPath", "");

    // 多ASR引擎默认配置
    if (!settings_->contains("asr/engineType")) settings_->setValue("asr/engineType", 0); // 0=Whisper
    if (!settings_->contains("asr/modelPath")) settings_->setValue("asr/modelPath", "");
    if (!settings_->contains("asr/enabled")) settings_->setValue("asr/enabled", false);

    // VAD 配置
    if (!settings_->contains("vad/enabled")) settings_->setValue("vad/enabled", false);
    if (!settings_->contains("vad/modelPath")) settings_->setValue("vad/modelPath", "");

    // 翻译配置
    if (!settings_->contains("translate/enabled")) settings_->setValue("translate/enabled", false);
    if (!settings_->contains("translate/type")) settings_->setValue("translate/type", 3); // 3=TencentCloud
    if (!settings_->contains("translate/targetLang")) settings_->setValue("translate/targetLang", "zh");
    if (!settings_->contains("translate/tencentSecretId")) settings_->setValue("translate/tencentSecretId", "");
    if (!settings_->contains("translate/tencentSecretKey")) settings_->setValue("translate/tencentSecretKey", "");

    // AI 视频总结默认配置
    if (!settings_->contains("summary/apiKey")) settings_->setValue("summary/apiKey", "");
    if (!settings_->contains("summary/modelEndpoint")) {
        settings_->setValue("summary/modelEndpoint", "https://dashscope.aliyuncs.com/compatible-mode/v1");
    }
    if (!settings_->contains("summary/segmentDuration")) settings_->setValue("summary/segmentDuration", 5000);
    if (!settings_->contains("summary/model")) settings_->setValue("summary/model", "qwen-vl-plus");
    if (!settings_->contains("summary/semanticEnabled")) settings_->setValue("summary/semanticEnabled", false);
    if (!settings_->contains("summary/semanticAudioWeight")) settings_->setValue("summary/semanticAudioWeight", 0.6);
    if (!settings_->contains("summary/semanticVideoWeight")) settings_->setValue("summary/semanticVideoWeight", 0.4);
    if (!settings_->contains("summary/semanticMinSegmentMs")) settings_->setValue("summary/semanticMinSegmentMs", 3000);
    if (!settings_->contains("summary/semanticMaxSegmentMs")) settings_->setValue("summary/semanticMaxSegmentMs", 120000);

    // 分析结果缓存 (默认开启)
    if (!settings_->contains("summary/cacheEnabled")) settings_->setValue("summary/cacheEnabled", true);

    QDir().mkpath(getThumbnailDir());
}

ConfigManager& ConfigManager::instance()
{
    static ConfigManager inst;
    return inst;
}

QString ConfigManager::configDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString ConfigManager::playlistFile() const
{
    return playlistFile_;
}

void ConfigManager::load()
{
    // playlist JSON
    QFile file(playlistFile_);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ConfigManager: Failed to open playlist for reading:" << playlistFile_
                   << "error:" << file.errorString();
        emit configLoaded();
        return;
    }
    QByteArray data = file.readAll();
    file.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "ConfigManager: Failed to parse playlist JSON:" << err.errorString();
        playlistData_ = QJsonObject();
    } else {
        playlistData_ = doc.object();
    }
    emit configLoaded();
}

void ConfigManager::save()
{
    QFile file(playlistFile_);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "ConfigManager: Failed to open playlist for writing:" << playlistFile_
                   << "error:" << file.errorString();
        return;
    }
    QByteArray data = QJsonDocument(playlistData_).toJson(QJsonDocument::Indented);
    qint64 written = file.write(data);
    file.close();
    if (written != data.size()) {
        qWarning() << "ConfigManager: Incomplete write to playlist";
    }
    settings_->sync();
}

// Settings accessors
bool ConfigManager::isHardware() const
{
    return settings_->value("hardware", false).toBool();
}
void ConfigManager::setHardware(bool value)
{
    settings_->setValue("hardware", value);
}

QString ConfigManager::getDecoderFormat() const
{
    return settings_->value("decoderFormat", "default").toString();
}
void ConfigManager::setDecoderFormat(const QString& format)
{
    settings_->setValue("decoderFormat", format);
}

int ConfigManager::getBrightness() const
{
    return settings_->value("brightness", 0).toInt();
}
void ConfigManager::setBrightness(int value)
{
    settings_->setValue("brightness", value);
}

int ConfigManager::getContrast() const
{
    return settings_->value("contrast", 100).toInt();
}
void ConfigManager::setContrast(int value)
{
    settings_->setValue("contrast", value);
}

int ConfigManager::getSaturation() const
{
    return settings_->value("saturation", 100).toInt();
}
void ConfigManager::setSaturation(int value)
{
    settings_->setValue("saturation", value);
}

int ConfigManager::getSubtitleFontSize() const
{
    return settings_->value("subtitleFontSize", 26).toInt();
}

void ConfigManager::setSubtitleFontSize(int size)
{
    settings_->setValue("subtitleFontSize", size);
}

int ConfigManager::getVideoSizeMode() const
{
    return settings_->value("videoSizeMode", 0).toInt();
}
void ConfigManager::setVideoSizeMode(int mode)
{
    settings_->setValue("videoSizeMode", mode);
}

QString ConfigManager::getScreenshotSavePath() const
{
    return settings_->value("screenshotSavePath", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
}
void ConfigManager::setScreenshotSavePath(const QString& path)
{
    settings_->setValue("screenshotSavePath", path);
}

QString ConfigManager::getModelPath() const
{
    // 兼容：优先读 asr/modelPath，回退到旧 modelPath
    QString v = settings_->value("asr/modelPath").toString();
    if (v.isEmpty()) v = settings_->value("modelPath", "").toString();
    return v;
}
void ConfigManager::setModelPath(const QString& path)
{
    settings_->setValue("asr/modelPath", path);
    // 同步写入旧 key 保证向后兼容
    settings_->setValue("modelPath", path);
}

// 多ASR引擎配置
int ConfigManager::getAsrEngineType() const
{
    // 兼容旧版本：优先读 asr/engineType，没有则默认 0
    return settings_->value("asr/engineType", 0).toInt();
}
void ConfigManager::setAsrEngineType(int type)
{
    settings_->setValue("asr/engineType", type);
}

bool ConfigManager::getAsrEnabled() const
{
    return settings_->value("asr/enabled", false).toBool();
}

void ConfigManager::setAsrEnabled(bool enabled)
{
    settings_->setValue("asr/enabled", enabled);
}

QString ConfigManager::getVadModelPath() const
{
    // 兼容：优先读 vad/modelPath，回退到旧 asr/vadModelPath
    QString v = settings_->value("vad/modelPath").toString();
    if (v.isEmpty()) v = settings_->value("asr/vadModelPath", "").toString();
    return v;
}
void ConfigManager::setVadModelPath(const QString& path)
{
    settings_->setValue("vad/modelPath", path);
}

bool ConfigManager::getVadEnabled() const
{
    // 兼容：优先读 vad/enabled，回退到旧 asr/vadEnabled
    if (settings_->contains("vad/enabled")) return settings_->value("vad/enabled").toBool();
    return settings_->value("asr/vadEnabled", false).toBool();
}
void ConfigManager::setVadEnabled(bool enabled)
{
    settings_->setValue("vad/enabled", enabled);
}

// 翻译配置
int ConfigManager::getTranslatorType() const
{
    return settings_->value("translate/type", 3).toInt();
}
void ConfigManager::setTranslatorType(int type)
{
    settings_->setValue("translate/type", type);
}

bool ConfigManager::getTranslationEnabled() const
{
    return settings_->value("translate/enabled", false).toBool();
}
void ConfigManager::setTranslationEnabled(bool enabled)
{
    settings_->setValue("translate/enabled", enabled);
}

QString ConfigManager::getTranslateTargetLang() const
{
    return settings_->value("translate/targetLang", "zh").toString();
}
void ConfigManager::setTranslateTargetLang(const QString& lang)
{
    settings_->setValue("translate/targetLang", lang);
}

// 腾讯翻译密钥
QString ConfigManager::getTencentSecretId() const
{
    return settings_->value("translate/tencentSecretId", "").toString();
}
void ConfigManager::setTencentSecretId(const QString& id)
{
    settings_->setValue("translate/tencentSecretId", id);
}

QString ConfigManager::getTencentSecretKey() const
{
    return settings_->value("translate/tencentSecretKey", "").toString();
}
void ConfigManager::setTencentSecretKey(const QString& key)
{
    settings_->setValue("translate/tencentSecretKey", key);
}

// Playlist
QList<ConfigManager::VideoItem> ConfigManager::getVideoList() const
{
    QList<VideoItem> result;
    QJsonArray arr = playlistData_.value("videos").toArray();
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        VideoItem item;
        item.path = obj.value("path").toString();
        item.name = obj.value("name").toString();
        item.duration = obj.value("duration").toInt();
        item.thumbnail = obj.value("thumbnail").toString();
        item.position = obj.value("position").toInteger();
        result.append(item);
    }
    return result;
}

void ConfigManager::setVideoList(const QList<VideoItem>& list)
{
    QJsonArray arr;
    for (const VideoItem& item : list) {
        QJsonObject obj;
        obj.insert("path", item.path);
        obj.insert("name", item.name);
        obj.insert("duration", item.duration);
        obj.insert("thumbnail", item.thumbnail);
        obj.insert("position", QJsonValue::fromVariant(QVariant(item.position)));
        arr.append(obj);
    }
    playlistData_.insert("videos", arr);
}

int ConfigManager::getCurrentIndex() const
{
    return playlistData_.value("currentIndex").toInt(0);
}

void ConfigManager::setCurrentIndex(int index)
{
    playlistData_.insert("currentIndex", index);
}

PlayMode ConfigManager::getPlayMode() const
{
    QString modeStr = playlistData_.value("playMode").toString("ListLoop");
    if (modeStr == "SingleRepeat") return PlayMode::SingleRepeat;
    if (modeStr == "Shuffle") return PlayMode::Shuffle;
    return PlayMode::ListLoop;
}

void ConfigManager::setPlayMode(PlayMode mode)
{
    QString modeStr;
    switch (mode) {
        case PlayMode::SingleRepeat: modeStr = "SingleRepeat"; break;
        case PlayMode::Shuffle: modeStr = "Shuffle"; break;
        default: modeStr = "ListLoop"; break;
    }
    playlistData_.insert("playMode", modeStr);
}

void ConfigManager::updateVideoPosition(const QString& path, qint64 pos)
{
    QJsonArray arr = playlistData_.value("videos").toArray();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        if (obj.value("path").toString() == path) {
            obj.insert("position", QJsonValue::fromVariant(QVariant(pos)));
            arr[i] = obj;
            break;
        }
    }
    playlistData_.insert("videos", arr);
}

QString ConfigManager::getThumbnailDir() const
{
    return configDir() + "/thumbnails/";
}

QString ConfigManager::thumbnailPathForVideo(const QString& videoPath) const
{
    QString hash = QString(QCryptographicHash::hash(videoPath.toUtf8(), QCryptographicHash::Md5).toHex());
    return getThumbnailDir() + hash + ".jpg";
}

QString ConfigManager::getSummaryApiKey() const
{
    return settings_->value("summary/apiKey", "").toString();
}

void ConfigManager::setSummaryApiKey(const QString& key)
{
    settings_->setValue("summary/apiKey", key);
}

QString ConfigManager::getSummaryModelEndpoint() const
{
    return settings_->value("summary/modelEndpoint",
        "https://dashscope.aliyuncs.com/compatible-mode/v1").toString();
}

void ConfigManager::setSummaryModelEndpoint(const QString& url)
{
    settings_->setValue("summary/modelEndpoint", url);
}

QString ConfigManager::getSummaryModel() const
{
    return settings_->value("summary/model", "qwen-vl-plus").toString();
}

void ConfigManager::setSummaryModel(const QString& model)
{
    settings_->setValue("summary/model", model);
}

int ConfigManager::getSummarySegmentDuration() const
{
    return settings_->value("summary/segmentDuration", 5000).toInt();
}

void ConfigManager::setSummarySegmentDuration(int ms)
{
    settings_->setValue("summary/segmentDuration", ms);
}

bool ConfigManager::getSemanticSegmentationEnabled() const
{
    return settings_->value("summary/semanticEnabled", false).toBool();
}

void ConfigManager::setSemanticSegmentationEnabled(bool enabled)
{
    settings_->setValue("summary/semanticEnabled", enabled);
}

double ConfigManager::getSemanticAudioWeight() const
{
    return settings_->value("summary/semanticAudioWeight", 0.6).toDouble();
}

void ConfigManager::setSemanticAudioWeight(double w)
{
    settings_->setValue("summary/semanticAudioWeight", w);
    settings_->setValue("summary/semanticVideoWeight", 1.0 - w);
}

double ConfigManager::getSemanticVideoWeight() const
{
    return settings_->value("summary/semanticVideoWeight", 0.4).toDouble();
}

void ConfigManager::setSemanticVideoWeight(double w)
{
    settings_->setValue("summary/semanticVideoWeight", w);
    settings_->setValue("summary/semanticAudioWeight", 1.0 - w);
}

int ConfigManager::getSemanticMinSegmentMs() const
{
    return settings_->value("summary/semanticMinSegmentMs", 3000).toInt();
}

void ConfigManager::setSemanticMinSegmentMs(int ms)
{
    settings_->setValue("summary/semanticMinSegmentMs", ms);
}

int ConfigManager::getSemanticMaxSegmentMs() const
{
    return settings_->value("summary/semanticMaxSegmentMs", 120000).toInt();
}

void ConfigManager::setSemanticMaxSegmentMs(int ms)
{
    settings_->setValue("summary/semanticMaxSegmentMs", ms);
}

bool ConfigManager::getSummaryCacheEnabled() const
{
    return settings_->value("summary/cacheEnabled", true).toBool();
}

void ConfigManager::setSummaryCacheEnabled(bool enabled)
{
    settings_->setValue("summary/cacheEnabled", enabled);
}

QString ConfigManager::getSummaryCacheDir() const
{
    QString dir = configDir() + "/summaries";
    QDir().mkpath(dir);
    return dir;
}

QString ConfigManager::computeVideoCacheKey(const QString& videoPath) const
{
    QFileInfo fi(videoPath);
    if (!fi.exists()) return {};
    QString raw = QString("%1|%2|%3")
        .arg(fi.absoluteFilePath())
        .arg(fi.size())
        .arg(fi.lastModified().toMSecsSinceEpoch());
    return QString(QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}
