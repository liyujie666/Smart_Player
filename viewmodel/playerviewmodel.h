#ifndef PLAYERVIEWMODEL_H
#define PLAYERVIEWMODEL_H

#include "iviewmodel.h"
#include "core/playercore.h"
#include <QByteArray>
#include <QString>

extern "C" {
#include <libavutil/pixfmt.h>
}

/**
 *  PlayerViewModel
 *
 *  封装 PlayerCore，对 View 仅暴露：
 *    - 可观察属性（Q_PROPERTY + NOTIFY）：state / position / duration / volume /
 *      mute / speedIndex / asrEnabled / hasAudio / hasVideo / fileUrl
 *    - 命令槽（public slots）：open/play/pause/stop/togglePlayPause/seek/
 *      seekRelative/setVolume/setMute/toggleMute/setSpeedIndex/takeScreenshot/
 *      setAsrEnabled/setModelPath/useHardware/setDecodeType/setScreenshotSavePath
 *    - 视频帧透传信号：frameYuv420p/Nv12/Rgba —— 帧数据本身是渲染数据流，
 *      不属于"可观察属性"，直接透传给渲染 View（OpenGLRenderer）即可。
 *
 *  注意：PlayerViewModel **不持有** PlayerCore 的所有权语义（用 raw ptr +
 *  外部 new/delete 也可），这里采用"VM 创建并拥有 Core"的模式，
 *  Core 的父对象设为本 VM，随 VM 析构。
 */
class PlayerViewModel : public IViewModel {
    Q_OBJECT
    Q_PROPERTY(State       state         READ state         NOTIFY stateChanged)
    Q_PROPERTY(qint64      position      READ currentPos    NOTIFY positionChanged)
    Q_PROPERTY(qint64      duration      READ duration      NOTIFY durationChanged)
    Q_PROPERTY(int         volume        READ volume        WRITE setVolume     NOTIFY volumeChanged)
    Q_PROPERTY(bool        mute          READ isMute        WRITE setMute       NOTIFY muteChanged)
    Q_PROPERTY(int         speedIndex    READ speedIndex    WRITE setSpeed      NOTIFY speedIndexChanged)
    Q_PROPERTY(bool        asrEnabled    READ isAsrEnabled  WRITE setAsrEnabled NOTIFY asrEnabledChanged)
    Q_PROPERTY(bool        hasAudio      READ hasAudio      NOTIFY mediaInfoChanged)
    Q_PROPERTY(bool        hasVideo      READ hasVideo      NOTIFY mediaInfoChanged)
    Q_PROPERTY(QString     fileUrl       READ fileUrl       NOTIFY fileUrlChanged)
    Q_PROPERTY(QString     currentSubtitle READ currentSubtitle NOTIFY subtitleChanged)

public:
    // 与 PlayerCore::State 一一对应；这里复制一份枚举是为了让 View 完全不
    // 依赖 core/ 头文件（未来想抽离 core 也方便）。
    // 采用旧式 enum 是为了让 View 写 `PlayerViewModel::Running` 时能像
    // `PlayerCore::Running` 一样自动转换为 int / 与 State 比较，方便阶段 1 迁移。
    enum State {
        Stopped = 0,
        Running = 1,
        Paused  = 2
    };
    Q_ENUM(State)
    using PlayerState = State;   // 旧别名（如果想用类型化枚举语义，可直接写 State）

    explicit PlayerViewModel(QObject* parent = nullptr);
    ~PlayerViewModel() override;

    // ===== 可观察属性 getter =====
    State       state() const          { return m_state; }
    // 沿用 PlayerCore 旧名（currentPos / duration），方便迁移；Q_PROPERTY 的 READ 也指向这里
    qint64      currentPos() const     { return m_position; }
    qint64      duration() const       { return m_duration; }
    int         volume() const         { return m_volume; }
    bool        isMute() const;
    int         speedIndex() const     { return m_speedIndex; }
    bool        isAsrEnabled() const;
    bool        hasAudio() const;
    bool        hasVideo() const;
    QString     fileUrl() const;
    QString     currentSubtitle() const { return m_currentSubtitle; }

    // 直通到 Core 的"原生句柄"——目前仅 VideoInfoDialog 等少量 View 需要。
    // 这些不暴露成 Q_PROPERTY，是为了避免 ViewModel 层语义被原生类型污染。
    // 阶段 1 之后会逐步收敛（例如 VideoInfoDialog 接受一个 VideoInfo 结构体而非 AVFormatContext*）。
    AVFormatContext*    avFormatContext() const;
    Demuxer::MediaType  mediaType() const;

    // 给暂时无法迁移的极少数模块（例如 VideoSummaryManager 需要 ASR 状态等）保留逃生口。
    // 新代码禁止使用此方法，请通过 VM 的属性 / slot / signal 完成交互。
    PlayerCore* core() const { return m_core; }

public slots:
    // ===== 命令 (Commands) =====
    void open(const QString& url);
    void play();
    void pause();
    void stop();
    void togglePlayPause();              // VM 层语义：按一个按钮在 play/pause/start-from-stopped 之间循环
    void seek(qint64 posUs);             // 与 PlayerCore::seek 单位一致（μs）
    void seekRelativeSec(int seconds);   // 以当前位置为基准，单位秒

    void setVolume(int v);
    void setMute(bool mute);
    void toggleMute();

    void setSpeed(int idx);    // 与 PlayerCore::setSpeed(int) 兼容（1=0.5x..4=2x）
    void setSpeedIndex(int idx) { setSpeed(idx); }   // 别名，便于 Q_PROPERTY WRITE

    void takeScreenshot();
    void setScreenshotSavePath(const QString& path);

    void useHardware(bool on);
    void setDecodeType(const QString& decoder);

    void setAsrEnabled(bool enabled);
    void setModelPath(const QString& path);

signals:
    // ===== 可观察属性 NOTIFY 信号（无参数版，View 收到后通过 getter 取最新值） =====
    // 这些信号刻意与 PlayerCore 同名/同签名，便于 MainWindow 渐进迁移：现有
    // connect(player_, &PlayerCore::stateChanged, ...) 改为绑定 VM 时只需改类型，
    // 不必改 slot 实现。VM 不强求 NOTIFY 带参（参数版未来按需补充）。
    void stateChanged();
    void timeChanged();
    void initFinished();
    void playFailed(const QString& info);
    void openResult(bool ok);
    void playFinished();
    void subtitleReady(const QString& text);
    void screecshotStatus(const QString& path, bool ok);  // 保留原名（兼容现有代码）

    // 新增 ViewModel 层 NOTIFY（属性级，View 想要细粒度可改用这些）：
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void volumeChanged(int v);
    void muteChanged(bool mute);
    void speedIndexChanged(int idx);
    void asrEnabledChanged(bool enabled);
    void mediaInfoChanged();
    void fileUrlChanged(const QString& url);
    void subtitleChanged(const QString& text);

    // ===== 视频帧透传（给渲染 View 用，VM 不解释内容） =====
    void frameYuv420pDecoded(const QByteArray& yuv, int w, int h);
    void frameNv12Decoded(const QByteArray& yuv,    int w, int h);
    void frameRGBADecoded(const QByteArray& rgb,    int w, int h);

private slots:
    void onCoreStateChanged();
    void onCoreTimeChanged();
    void onCoreInitFinished();
    void onCorePlayFailed(const QString& info);
    void onCoreOpenResult(bool ok);
    void onCorePlayFinished();
    void onCoreSubtitleReady(const QString& text);
    void onCoreScreenshotStatus(const QString& path, bool ok);

private:
    static PlayerState toVmState(PlayerCore::State s);

    PlayerCore* m_core = nullptr;

    // 缓存的属性值（避免每次 currentPos()/duration() 都跨线程问 core）
    PlayerState m_state    = PlayerState::Stopped;
    qint64      m_position = 0;
    qint64      m_duration = 0;
    int         m_volume   = 50;
    int         m_speedIndex = 2;            // 1=0.5x, 2=1x, 3=1.5x, 4=2x
    QString     m_currentSubtitle;
};

#endif // PLAYERVIEWMODEL_H
