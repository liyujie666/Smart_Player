#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <QList>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>

enum class PlayMode {
    ListLoop,
    SingleRepeat,
    Shuffle
};

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    static ConfigManager& instance();

    void load();
    void save();

    // Settings (QSettings / INI)
    bool isHardware() const;
    void setHardware(bool value);

    QString getDecoderFormat() const;
    void setDecoderFormat(const QString& format);

    int getBrightness() const;
    void setBrightness(int value);

    int getContrast() const;
    void setContrast(int value);

    int getSaturation() const;
    void setSaturation(int value);

    int getVideoSizeMode() const;
    void setVideoSizeMode(int mode);

    QString getScreenshotSavePath() const;
    void setScreenshotSavePath(const QString& path);

    QString getModelPath() const;
    void setModelPath(const QString& path);

    // Playlist (JSON)
    struct VideoItem {
        QString path;
        QString name;
        int duration;     // seconds
        QString thumbnail; // relative path under thumbnails dir
        qint64 position;  // ms
    };
    QList<VideoItem> getVideoList() const;
    void setVideoList(const QList<VideoItem>& list);

    int getCurrentIndex() const;
    void setCurrentIndex(int index);

    PlayMode getPlayMode() const;
    void setPlayMode(PlayMode mode);

    void updateVideoPosition(const QString& path, qint64 pos);

    QString getThumbnailDir() const;
    QString thumbnailPathForVideo(const QString& videoPath) const;

signals:
    void configLoaded();

private:
    ConfigManager();
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    QString configDir() const;
    QString playlistFile() const;

    QSettings* settings_ = nullptr;
    QString playlistFile_;
    QJsonObject playlistData_;
};

#endif // CONFIGMANAGER_H
