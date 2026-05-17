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
    if (!settings_->contains("videoSizeMode")) settings_->setValue("videoSizeMode", 0);
    if (!settings_->contains("screenshotSavePath")) {
        settings_->setValue("screenshotSavePath", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    }
    if (!settings_->contains("modelPath")) settings_->setValue("modelPath", "");

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
    return settings_->value("modelPath", "").toString();
}
void ConfigManager::setModelPath(const QString& path)
{
    settings_->setValue("modelPath", path);
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
