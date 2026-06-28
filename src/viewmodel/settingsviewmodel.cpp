#include "settingsviewmodel.h"

SettingsViewModel& SettingsViewModel::instance() {
    static SettingsViewModel s_inst;
    return s_inst;
}

SettingsViewModel::SettingsViewModel(QObject* parent) : IViewModel(parent) {
    // 首次访问时确保底层 ConfigManager 已经从磁盘加载过一次。
    // ConfigManager 在 MainWindow 构造期会显式 load()，这里再调一次是幂等的。
    // ConfigManager::instance().load();
}

// ============================================================
// Setters：每个 setter 内做"值未变直接 return"以防回路。
// 写入 ConfigManager 后主动 emit NOTIFY + anyChanged。
// 注意：ConfigManager::set* 内部会写 QSettings，但不会自动 save()；
//      saveToDisk() 才把它落盘。
// ============================================================
void SettingsViewModel::setHardware(bool v) {
    if (ConfigManager::instance().isHardware() == v) return;
    ConfigManager::instance().setHardware(v);
    emit hardwareChanged(v);
    emit anyChanged();
}

void SettingsViewModel::setDecoderFormat(const QString& f) {
    if (ConfigManager::instance().getDecoderFormat() == f) return;
    ConfigManager::instance().setDecoderFormat(f);
    emit decoderFormatChanged(f);
    emit anyChanged();
}

void SettingsViewModel::setBrightness(int v) {
    if (ConfigManager::instance().getBrightness() == v) return;
    ConfigManager::instance().setBrightness(v);
    emit brightnessChanged(v);
    emit anyChanged();
}

void SettingsViewModel::setContrast(int v) {
    if (ConfigManager::instance().getContrast() == v) return;
    ConfigManager::instance().setContrast(v);
    emit contrastChanged(v);
    emit anyChanged();
}

void SettingsViewModel::setSaturation(int v) {
    if (ConfigManager::instance().getSaturation() == v) return;
    ConfigManager::instance().setSaturation(v);
    emit saturationChanged(v);
    emit anyChanged();
}

void SettingsViewModel::setVideoSizeMode(int mode) {
    if (ConfigManager::instance().getVideoSizeMode() == mode) return;
    ConfigManager::instance().setVideoSizeMode(mode);
    emit videoSizeModeChanged(mode);
    emit anyChanged();
}

void SettingsViewModel::setSubtitleFontSize(int size) {
    if (ConfigManager::instance().getSubtitleFontSize() == size) return;
    ConfigManager::instance().setSubtitleFontSize(size);
    emit subtitleFontSizeChanged(size);
    emit anyChanged();
}

void SettingsViewModel::setScreenshotSavePath(const QString& path) {
    if (ConfigManager::instance().getScreenshotSavePath() == path) return;
    ConfigManager::instance().setScreenshotSavePath(path);
    emit screenshotSavePathChanged(path);
    emit anyChanged();
}

void SettingsViewModel::setModelPath(const QString& path) {
    if (ConfigManager::instance().getModelPath() == path) return;
    ConfigManager::instance().setModelPath(path);
    emit modelPathChanged(path);
    emit anyChanged();
}

void SettingsViewModel::setSummaryApiKey(const QString& k) {
    if (ConfigManager::instance().getSummaryApiKey() == k) return;
    ConfigManager::instance().setSummaryApiKey(k);
    emit summaryConfigChanged();
    emit anyChanged();
}

void SettingsViewModel::setSummaryEndpoint(const QString& url) {
    if (ConfigManager::instance().getSummaryModelEndpoint() == url) return;
    ConfigManager::instance().setSummaryModelEndpoint(url);
    emit summaryConfigChanged();
    emit anyChanged();
}

void SettingsViewModel::setSummaryModel(const QString& m) {
    if (ConfigManager::instance().getSummaryModel() == m) return;
    ConfigManager::instance().setSummaryModel(m);
    emit summaryConfigChanged();
    emit anyChanged();
}

void SettingsViewModel::setSummarySegmentDurationMs(int ms) {
    if (ConfigManager::instance().getSummarySegmentDuration() == ms) return;
    ConfigManager::instance().setSummarySegmentDuration(ms);
    emit summaryConfigChanged();
    emit anyChanged();
}

void SettingsViewModel::setSemanticSegEnabled(bool on) {
    if (ConfigManager::instance().getSemanticSegmentationEnabled() == on) return;
    ConfigManager::instance().setSemanticSegmentationEnabled(on);
    emit summaryConfigChanged();
    emit anyChanged();
}

void SettingsViewModel::setSummaryCacheEnabled(bool on) {
    if (ConfigManager::instance().getSummaryCacheEnabled() == on) return;
    ConfigManager::instance().setSummaryCacheEnabled(on);
    emit summaryConfigChanged();
    emit anyChanged();
}

// ============================================================
// Persistence
// ============================================================
void SettingsViewModel::saveToDisk() {
    ConfigManager::instance().save();
}

void SettingsViewModel::loadFromDisk() {
    ConfigManager::instance().load();
    // 加载后底层值可能变化，逐个 emit 让监听方刷新。这里采用"宽信号"策略：
    emit anyChanged();
}
