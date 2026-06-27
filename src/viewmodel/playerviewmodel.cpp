#include "playerviewmodel.h"
#include <QDebug>
#include <QFileInfo>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

PlayerViewModel::PlayerViewModel(QObject* parent)
    : IViewModel(parent)
{
    // VM 拥有 Core 的生命周期（QObject parent 机制自动释放）
    m_core = new PlayerCore(this);

    // ====== Core -> VM 信号桥接 ======
    connect(m_core, &PlayerCore::stateChanged,         this, &PlayerViewModel::onCoreStateChanged);
    connect(m_core, &PlayerCore::timeChanged,          this, &PlayerViewModel::onCoreTimeChanged);
    connect(m_core, &PlayerCore::initFinished,         this, &PlayerViewModel::onCoreInitFinished);
    connect(m_core, &PlayerCore::playFailed,           this, &PlayerViewModel::onCorePlayFailed);
    connect(m_core, &PlayerCore::openResult,           this, &PlayerViewModel::onCoreOpenResult);
    connect(m_core, &PlayerCore::playFinished,         this, &PlayerViewModel::onCorePlayFinished);
    connect(m_core, &PlayerCore::subtitleReady,        this, &PlayerViewModel::onCoreSubtitleReady);
    connect(m_core, &PlayerCore::screecshotStatus,     this, &PlayerViewModel::onCoreScreenshotStatus);

    // 视频帧 —— 直接透传（不做任何拷贝/重组），让渲染 View 自己 connect VM
    connect(m_core, &PlayerCore::frameYuv420pDecoded,  this, &PlayerViewModel::frameYuv420pDecoded);
    connect(m_core, &PlayerCore::frameNv12Decoded,     this, &PlayerViewModel::frameNv12Decoded);
    connect(m_core, &PlayerCore::frameRGBADecoded,     this, &PlayerViewModel::frameRGBADecoded);
}

PlayerViewModel::~PlayerViewModel() = default;

// ============== 属性 getter（透传到 Core）==============
bool PlayerViewModel::isMute() const       { return m_core ? m_core->isMute() : false; }
bool PlayerViewModel::isAsrEnabled() const { return m_core ? m_core->isAsrEnabled() : false; }
bool PlayerViewModel::hasAudio() const     { return m_core ? m_core->hasAudio() : false; }
bool PlayerViewModel::hasVideo() const     { return m_core ? m_core->hasVideo() : false; }
QString PlayerViewModel::fileUrl() const   { return m_core ? m_core->fileUrl() : QString(); }

Demuxer::MediaType PlayerViewModel::mediaType() const {
    return m_core ? m_core->mediaType() : Demuxer::MediaType::FILE_TYPE;
}

// ============== 命令 ==============
void PlayerViewModel::open(const QString& url) {
    if (!m_core) return;
    m_core->open(url);
    emit fileUrlChanged(url);
}

void PlayerViewModel::play()   { if (m_core) m_core->play(); }
void PlayerViewModel::pause()  { if (m_core) m_core->pause(); }
void PlayerViewModel::stop()   { if (m_core) m_core->stop(); }

void PlayerViewModel::togglePlayPause() {
    if (!m_core) return;
    switch (m_core->state()) {
    case PlayerCore::Running: m_core->pause(); break;
    case PlayerCore::Paused:  m_core->play();  break;
    case PlayerCore::Stopped: /* View 决定是否从列表第 N 首开始；VM 不做列表逻辑 */ break;
    }
}

void PlayerViewModel::seek(qint64 posUs) {
    if (m_core) m_core->seek(posUs);
}

void PlayerViewModel::seekRelativeSec(int seconds) {
    if (!m_core) return;
    // PlayerCore::currentPos() 返回 ms；seek() 接收 μs。这里保留与 MainWindow
    // 旧 seekRelative 完全一致的语义。
    qint64 cur_ms = m_core->currentPos();
    qint64 dur_ms = m_core->duration();
    qint64 target_ms = cur_ms + qint64(seconds) * 1000;
    if (target_ms < 0)       target_ms = 0;
    if (dur_ms > 0 && target_ms > dur_ms) target_ms = dur_ms;
    m_core->seek(target_ms * 1000); // μs
}

void PlayerViewModel::setVolume(int v) {
    if (v == m_volume) return;
    m_volume = v;
    if (m_core) m_core->setVolume(v);
    emit volumeChanged(v);
}

void PlayerViewModel::setMute(bool mute) {
    if (!m_core) return;
    if (m_core->isMute() == mute) return;
    m_core->setMute(mute);
    emit muteChanged(mute);
}

void PlayerViewModel::toggleMute() {
    if (!m_core) return;
    setMute(!m_core->isMute());
}

void PlayerViewModel::setSpeed(int idx) {
    if (idx == m_speedIndex) return;
    m_speedIndex = idx;
    if (m_core) m_core->setSpeed(idx);
    emit speedIndexChanged(idx);
}

void PlayerViewModel::takeScreenshot() {
    if (!m_core) return;
    if (m_core->state() == PlayerCore::Stopped) return;
    m_core->takeScreenshot();
}

void PlayerViewModel::setScreenshotSavePath(const QString& path) {
    if (m_core) m_core->setScreenshotSavePath(path);
}

void PlayerViewModel::useHardware(bool on) {
    if (m_core) m_core->useHardware(on);
}

void PlayerViewModel::setDecodeType(const QString& decoder) {
    if (m_core) m_core->setDecodeType(decoder);
}

void PlayerViewModel::setAsrEnabled(bool enabled) {
    if (!m_core) return;
    if (m_core->isAsrEnabled() == enabled) return;
    m_core->setAsrEnabled(enabled);
    emit asrEnabledChanged(enabled);
}

void PlayerViewModel::setModelPath(const QString& path) {
    if (m_core) m_core->setModelPath(path);
}

// ============== Core -> VM 信号处理 ==============
void PlayerViewModel::onCoreStateChanged() {
    if (!m_core) return;
    PlayerState s = toVmState(m_core->state());
    if (s != m_state) {
        m_state = s;
    }
    // 总是 emit，兼容旧的"无参 stateChanged()" 语义
    emit stateChanged();
}

void PlayerViewModel::onCoreTimeChanged() {
    if (!m_core) return;
    qint64 p = m_core->currentPos();
    if (p != m_position) {
        m_position = p;
        emit positionChanged(p);
    }
    emit timeChanged();  // 兼容旧 PlayerCore::timeChanged()
}

void PlayerViewModel::onCoreInitFinished() {
    if (!m_core) return;
    qint64 d = m_core->duration();
    if (d != m_duration) {
        m_duration = d;
        emit durationChanged(d);
    }

    // 从 AVFormatContext 一次性提炼出 MediaInfo DTO，之后 View 都用 DTO，
    // 不再通过 VM 拿原生指针（playerviewmodel::avFormatContext 已删除）。
    m_mediaInfo = MediaInfo{};
    AVFormatContext* fmt = m_core->avFormatContext();
    if (fmt) {
        m_mediaInfo.filePath = m_core->fileUrl();
        QFileInfo fi(m_mediaInfo.filePath);
        m_mediaInfo.fileName = fi.fileName();
        if (fmt->iformat && fmt->iformat->name) {
            m_mediaInfo.formatName = QString::fromLatin1(fmt->iformat->name);
        }
        int64_t dur_us = fmt->duration;
        if (dur_us != AV_NOPTS_VALUE && dur_us > 0) {
            m_mediaInfo.durationMs = dur_us / 1000;
        }
        int64_t br = fmt->bit_rate;
        if (br > 0 && br != AV_NOPTS_VALUE) m_mediaInfo.bitRate = br;

        int videoIndex = -1, audioIndex = -1;
        for (unsigned i = 0; i < fmt->nb_streams; ++i) {
            AVMediaType t = fmt->streams[i]->codecpar->codec_type;
            if (t == AVMEDIA_TYPE_VIDEO && videoIndex < 0) videoIndex = int(i);
            else if (t == AVMEDIA_TYPE_AUDIO && audioIndex < 0) audioIndex = int(i);
        }
        if (videoIndex >= 0) {
            m_mediaInfo.hasVideo = true;
            AVPixelFormat pix = (AVPixelFormat)fmt->streams[videoIndex]->codecpar->format;
            if (const char* nm = av_get_pix_fmt_name(pix)) {
                m_mediaInfo.videoPixelFormat = QString::fromLatin1(nm);
            }
            AVRational fr = fmt->streams[videoIndex]->codecpar->framerate;
            if (fr.den > 0) m_mediaInfo.videoFrameRate = double(fr.num) / double(fr.den);
        }
        if (audioIndex >= 0) {
            m_mediaInfo.hasAudio = true;
            m_mediaInfo.audioChannels   = fmt->streams[audioIndex]->codecpar->ch_layout.nb_channels;
            m_mediaInfo.audioSampleRate = fmt->streams[audioIndex]->codecpar->sample_rate;
        }
    }

    emit mediaInfoChanged();
    emit initFinished();
}

void PlayerViewModel::onCorePlayFailed(const QString& info) {
    emit playFailed(info);
}

void PlayerViewModel::onCoreOpenResult(bool ok) {
    emit openResult(ok);
}

void PlayerViewModel::onCorePlayFinished() {
    emit playFinished();
}

void PlayerViewModel::onCoreSubtitleReady(const QString& text) {
    if (text != m_currentSubtitle) {
        m_currentSubtitle = text;
        emit subtitleChanged(text);
    }
    emit subtitleReady(text); // 兼容旧名
}

void PlayerViewModel::onCoreScreenshotStatus(const QString& path, bool ok) {
    emit screecshotStatus(path, ok);
}

PlayerViewModel::PlayerState PlayerViewModel::toVmState(PlayerCore::State s) {
    switch (s) {
    case PlayerCore::Stopped: return PlayerState::Stopped;
    case PlayerCore::Running: return PlayerState::Running;
    case PlayerCore::Paused:  return PlayerState::Paused;
    }
    return PlayerState::Stopped;
}
