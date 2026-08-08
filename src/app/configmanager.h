#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <QList>
#include <QFileInfo>
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

    int getSubtitleFontSize() const;
    void setSubtitleFontSize(int size);

    int getVideoSizeMode() const;
    void setVideoSizeMode(int mode);

    QString getScreenshotSavePath() const;
    void setScreenshotSavePath(const QString& path);

    QString getModelPath() const;
    void setModelPath(const QString& path);

    // 多ASR引擎配置
    int getAsrEngineType() const;          // 0=Whisper, 1=SenseVoice, 2=CloudASR
    void setAsrEngineType(int type);

    // 字幕开关持久化（程序级，跨视频保持）
    bool getAsrEnabled() const;
    void setAsrEnabled(bool enabled);

    QString getVadModelPath() const;
    void setVadModelPath(const QString& path);

    bool getVadEnabled() const;
    void setVadEnabled(bool enabled);

    // 翻译配置
    int getTranslatorType() const;         // 0=GPT, 1=NLLB, 2=MarianMT, 3=TencentCloud
    void setTranslatorType(int type);

    bool getTranslationEnabled() const;
    void setTranslationEnabled(bool enabled);

    QString getTranslateTargetLang() const;
    void setTranslateTargetLang(const QString& lang);

    // 腾讯翻译密钥
    QString getTencentSecretId() const;
    void setTencentSecretId(const QString& id);

    QString getTencentSecretKey() const;
    void setTencentSecretKey(const QString& key);

    // AI 视频总结配置
    QString getSummaryApiKey() const;
    void setSummaryApiKey(const QString& key);

    QString getSummaryModelEndpoint() const;
    void setSummaryModelEndpoint(const QString& url);

    QString getSummaryModel() const;
    void setSummaryModel(const QString& model);

    int getSummarySegmentDuration() const;
    void setSummarySegmentDuration(int ms);

    bool getSemanticSegmentationEnabled() const;
    void setSemanticSegmentationEnabled(bool enabled);

    double getSemanticAudioWeight() const;
    void setSemanticAudioWeight(double w);

    double getSemanticVideoWeight() const;
    void setSemanticVideoWeight(double w);

    int getSemanticMinSegmentMs() const;
    void setSemanticMinSegmentMs(int ms);

    int getSemanticMaxSegmentMs() const;
    void setSemanticMaxSegmentMs(int ms);

    // 分析结果缓存
    bool getSummaryCacheEnabled() const;
    void setSummaryCacheEnabled(bool enabled);

    QString getSummaryCacheDir() const;

    // 视频 -> 稳定缓存 ID (path|size|mtime -> SHA256 截前 16 位)
    QString computeVideoCacheKey(const QString& videoPath) const;

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
