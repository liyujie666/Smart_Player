#ifndef SETTINGSVIEWMODEL_H
#define SETTINGSVIEWMODEL_H

#include "iviewmodel.h"
#include "app/configmanager.h"

#include <QString>

/**
 *  SettingsViewModel
 *
 *  ConfigManager 的 ViewModel 包装层（单例）：
 *    - 所有 getter/setter 透传到 ConfigManager；
 *    - 每个属性提供 Q_PROPERTY + NOTIFY signal，且在 setter 内部主动 emit；
 *    - 用户在对话框上的"取消"由对话框侧自己负责状态回滚，VM 不强制提供 revert/commit。
 *
 *  阶段 4 的工作量做最小化处理：
 *    - 对话框 (settingDialog / SummarySettingsDialog) 暂保持现状直接读写 ConfigManager；
 *    - MainWindow / 其它"观察方"可以改为订阅 SettingsViewModel 的 NOTIFY，逐步迁移；
 *    - 后续如果想让对话框完全只跟 VM 对话，可在 dialog 内 connect VM 信号 + 在控件改动
 *      时调用 vm.setXxx()。
 */
class SettingsViewModel : public IViewModel {
    Q_OBJECT
    // 解码 / 显示
    Q_PROPERTY(bool    hardware           READ hardware           WRITE setHardware           NOTIFY hardwareChanged)
    Q_PROPERTY(QString decoderFormat      READ decoderFormat      WRITE setDecoderFormat      NOTIFY decoderFormatChanged)
    Q_PROPERTY(int     brightness         READ brightness         WRITE setBrightness         NOTIFY brightnessChanged)
    Q_PROPERTY(int     contrast           READ contrast           WRITE setContrast           NOTIFY contrastChanged)
    Q_PROPERTY(int     saturation         READ saturation         WRITE setSaturation         NOTIFY saturationChanged)
    Q_PROPERTY(int     videoSizeMode      READ videoSizeMode      WRITE setVideoSizeMode      NOTIFY videoSizeModeChanged)
    Q_PROPERTY(int     subtitleFontSize   READ subtitleFontSize   WRITE setSubtitleFontSize   NOTIFY subtitleFontSizeChanged)

    // 路径
    Q_PROPERTY(QString screenshotSavePath READ screenshotSavePath WRITE setScreenshotSavePath NOTIFY screenshotSavePathChanged)
    Q_PROPERTY(QString modelPath          READ modelPath          WRITE setModelPath          NOTIFY modelPathChanged)

    // AI 总结
    Q_PROPERTY(QString summaryApiKey      READ summaryApiKey      WRITE setSummaryApiKey      NOTIFY summaryConfigChanged)
    Q_PROPERTY(QString summaryEndpoint    READ summaryEndpoint    WRITE setSummaryEndpoint    NOTIFY summaryConfigChanged)
    Q_PROPERTY(QString summaryModel       READ summaryModel       WRITE setSummaryModel       NOTIFY summaryConfigChanged)
    Q_PROPERTY(int     summarySegmentDurationMs READ summarySegmentDurationMs WRITE setSummarySegmentDurationMs NOTIFY summaryConfigChanged)
    Q_PROPERTY(bool    semanticSegEnabled READ semanticSegEnabled WRITE setSemanticSegEnabled NOTIFY summaryConfigChanged)
    Q_PROPERTY(bool    summaryCacheEnabled READ summaryCacheEnabled WRITE setSummaryCacheEnabled NOTIFY summaryConfigChanged)

public:
    static SettingsViewModel& instance();

    // ===== getter =====
    bool    hardware()           const { return ConfigManager::instance().isHardware(); }
    QString decoderFormat()      const { return ConfigManager::instance().getDecoderFormat(); }
    int     brightness()         const { return ConfigManager::instance().getBrightness(); }
    int     contrast()           const { return ConfigManager::instance().getContrast(); }
    int     saturation()         const { return ConfigManager::instance().getSaturation(); }
    int     videoSizeMode()      const { return ConfigManager::instance().getVideoSizeMode(); }
    int     subtitleFontSize()   const { return ConfigManager::instance().getSubtitleFontSize(); }
    QString screenshotSavePath() const { return ConfigManager::instance().getScreenshotSavePath(); }
    QString modelPath()          const { return ConfigManager::instance().getModelPath(); }

    QString summaryApiKey()         const { return ConfigManager::instance().getSummaryApiKey(); }
    QString summaryEndpoint()       const { return ConfigManager::instance().getSummaryModelEndpoint(); }
    QString summaryModel()          const { return ConfigManager::instance().getSummaryModel(); }
    int     summarySegmentDurationMs() const { return ConfigManager::instance().getSummarySegmentDuration(); }
    bool    semanticSegEnabled()    const { return ConfigManager::instance().getSemanticSegmentationEnabled(); }
    bool    summaryCacheEnabled()   const { return ConfigManager::instance().getSummaryCacheEnabled(); }

public slots:
    // ===== setter =====
    void setHardware(bool v);
    void setDecoderFormat(const QString& f);
    void setBrightness(int v);
    void setContrast(int v);
    void setSaturation(int v);
    void setVideoSizeMode(int mode);
    void setSubtitleFontSize(int size);
    void setScreenshotSavePath(const QString& path);
    void setModelPath(const QString& path);

    void setSummaryApiKey(const QString& k);
    void setSummaryEndpoint(const QString& url);
    void setSummaryModel(const QString& m);
    void setSummarySegmentDurationMs(int ms);
    void setSemanticSegEnabled(bool on);
    void setSummaryCacheEnabled(bool on);

    // 持久化（薄壳）：把内存中的 ConfigManager 写盘
    void saveToDisk();
    void loadFromDisk();

signals:
    void hardwareChanged(bool v);
    void decoderFormatChanged(const QString& f);
    void brightnessChanged(int v);
    void contrastChanged(int v);
    void saturationChanged(int v);
    void videoSizeModeChanged(int mode);
    void subtitleFontSizeChanged(int size);
    void screenshotSavePathChanged(const QString& path);
    void modelPathChanged(const QString& path);

    void summaryConfigChanged();    // AI 总结组：聚合信号，避免发出 6 个独立信号

    void anyChanged();              // 任何属性变化都会 emit（便于"应用"按钮启用判定）

private:
    explicit SettingsViewModel(QObject* parent = nullptr);
    ~SettingsViewModel() override = default;
    SettingsViewModel(const SettingsViewModel&) = delete;
    SettingsViewModel& operator=(const SettingsViewModel&) = delete;
};

#endif // SETTINGSVIEWMODEL_H
