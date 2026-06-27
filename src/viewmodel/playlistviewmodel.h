#ifndef PLAYLISTVIEWMODEL_H
#define PLAYLISTVIEWMODEL_H

#include "iviewmodel.h"
#include "app/configmanager.h"   // 复用 PlayMode 与 ConfigManager 持久化
#include <QList>
#include <QString>
#include <QVector>

/**
 *  PlaylistViewModel
 *
 *  播放列表的 ViewModel：
 *    - 数据：tracks（path + name + duration + thumbnailPath）+ currentIndex + playMode
 *    - 命令：增/删/清空/切换上一首下一首/切换播放模式/切换当前项
 *    - 持久化：通过 ConfigManager 进行（load/save 由 View 触发，VM 内部不订阅文件系统）
 *
 *  与 PlayerViewModel 的关系：完全解耦。
 *    - VM 不负责"实际开始播放"——它只发出 currentTrackRequested(path) 表达"用户希望开始播放这一项"。
 *      由外部（MainWindow 协调层 / 未来的 Controller）订阅并触发 PlayerViewModel::open()。
 *    - 这样 PlaylistViewModel 可以被无 PlayerViewModel 的 UI 测试用例直接复用。
 *
 *  与 View（QListWidget）的关系：单向数据流。
 *    - View 只观察 VM 的 trackAdded/trackRemoved/cleared/tracksReset/currentIndexChanged，
 *      在收到这些信号后增删自己持有的 QListWidgetItem。
 *    - View 上用户的"双击列表项"→ 调用 vm->setCurrentByPath(path)，VM 内部更新索引 + emit currentTrackRequested。
 */
class PlaylistViewModel : public IViewModel {
    Q_OBJECT

public:
    struct Track {
        QString path;          // 绝对路径（VM 内唯一性以此判定）
        QString name;          // 显示名（一般是文件名）
        int     durationSec = 0;
        QString thumbnailPath; // 缓存缩略图的绝对路径；为空表示尚未生成
    };

    explicit PlaylistViewModel(QObject* parent = nullptr);
    ~PlaylistViewModel() override = default;

    // ===== 只读访问 =====
    int                 count() const         { return m_tracks.size(); }
    bool                isEmpty() const       { return m_tracks.isEmpty(); }
    int                 currentIndex() const  { return m_currentIndex; }
    QString             currentPath() const;
    const Track*        at(int i) const;
    const QList<Track>& tracks() const        { return m_tracks; }
    bool                contains(const QString& path) const;
    int                 indexOf(const QString& path) const;
    PlayMode            playMode() const      { return m_playMode; }

public slots:
    // ===== 命令 =====
    // 增：单条；返回插入后的索引，如果重复返回原索引
    int  addTrack(const Track& t);
    // 增：批量；返回新增条数（重复的会跳过）
    int  addTracks(const QList<Track>& ts);
    // 删：按路径删除（删完会调整 currentIndex 到一个合法位置或 -1）
    void removeByPath(const QString& path);
    void clear();

    // 设置当前项；如果 emitRequest=true（默认），会 emit currentTrackRequested
    void setCurrentIndex(int idx, bool emitRequest = true);
    void setCurrentByPath(const QString& path, bool emitRequest = true);

    // 上一首 / 下一首（受 playMode 影响：列表循环；其它模式仍走顺序）
    void next();
    void previous();

    // 当前曲目播放完毕：根据 playMode 计算下一首
    void advanceForFinish();

    // 播放模式：按 ListLoop -> SingleRepeat -> Shuffle -> ListLoop 循环
    void togglePlayMode();
    void setPlayMode(PlayMode mode);

    // ===== 持久化（薄壳，转发到 ConfigManager） =====
    // 从 ConfigManager 加载（替换当前 tracks/currentIndex/playMode 并 emit 一组信号）
    void loadFromConfig();
    // 把当前状态写回 ConfigManager（不会调用 ConfigManager::save，由 View 决定是否持久化到磁盘）
    void writeToConfig() const;

signals:
    // ===== 列表结构变化 =====
    void trackAdded(int index, const PlaylistViewModel::Track& t);
    void trackRemoved(int index);
    void cleared();
    void tracksReset();             // loadFromConfig 等批量重置后发出

    // ===== 状态变化 =====
    void currentIndexChanged(int index);
    void playModeChanged(PlayMode mode);

    // ===== 用户意图 =====
    // VM 决定"应该播放哪一首"时发出，View / Controller 负责真正调用 PlayerViewModel::open
    void currentTrackRequested(const QString& path);

private:
    void rebuildShuffleIfNeeded();          // 切到 Shuffle 时初始化乱序表

private:
    QList<Track> m_tracks;
    int          m_currentIndex = -1;
    PlayMode     m_playMode     = PlayMode::ListLoop;

    // Shuffle 模式专用：m_tracks 的乱序快照（保存路径而非索引，便于 m_tracks 增删后保持合理性）
    QVector<QString> m_shuffle;
    int              m_shuffleCursor = 0;
};

#endif // PLAYLISTVIEWMODEL_H
