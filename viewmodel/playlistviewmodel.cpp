#include "playlistviewmodel.h"
#include <QFileInfo>
#include <algorithm>
#include <random>

PlaylistViewModel::PlaylistViewModel(QObject* parent)
    : IViewModel(parent)
{}

// ============== 只读访问 ==============
QString PlaylistViewModel::currentPath() const {
    if (m_currentIndex < 0 || m_currentIndex >= m_tracks.size()) return QString();
    return m_tracks[m_currentIndex].path;
}

const PlaylistViewModel::Track* PlaylistViewModel::at(int i) const {
    if (i < 0 || i >= m_tracks.size()) return nullptr;
    return &m_tracks[i];
}

bool PlaylistViewModel::contains(const QString& path) const {
    return indexOf(path) >= 0;
}

int PlaylistViewModel::indexOf(const QString& path) const {
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].path == path) return i;
    }
    return -1;
}

// ============== 命令 ==============
int PlaylistViewModel::addTrack(const Track& t) {
    int existed = indexOf(t.path);
    if (existed >= 0) return existed;

    int newIndex = m_tracks.size();
    m_tracks.append(t);
    emit trackAdded(newIndex, m_tracks.last());

    // 列表为空时第一次插入会把当前项指向 0
    if (m_currentIndex < 0) {
        m_currentIndex = 0;
        emit currentIndexChanged(m_currentIndex);
    }

    // Shuffle 模式下乱序表也要扩展
    if (m_playMode == PlayMode::Shuffle) {
        m_shuffle.append(t.path);
    }
    return newIndex;
}

int PlaylistViewModel::addTracks(const QList<Track>& ts) {
    int added = 0;
    for (const Track& t : ts) {
        int before = m_tracks.size();
        addTrack(t);
        if (m_tracks.size() != before) ++added;
    }
    return added;
}

void PlaylistViewModel::removeByPath(const QString& path) {
    int idx = indexOf(path);
    if (idx < 0) return;

    m_tracks.removeAt(idx);
    emit trackRemoved(idx);

    // 调整 currentIndex
    int newIdx = m_currentIndex;
    if (m_tracks.isEmpty()) {
        newIdx = -1;
    } else if (idx == m_currentIndex) {
        // 删的是当前项 → 留在原位置（即下一个），越界则回到 0
        if (newIdx >= m_tracks.size()) newIdx = 0;
    } else if (idx < m_currentIndex) {
        --newIdx;
    }
    if (newIdx != m_currentIndex) {
        m_currentIndex = newIdx;
        emit currentIndexChanged(newIdx);
    }

    // Shuffle 乱序表同步
    if (m_playMode == PlayMode::Shuffle) {
        int si = m_shuffle.indexOf(path);
        if (si >= 0) {
            m_shuffle.removeAt(si);
            if (m_shuffleCursor > si) --m_shuffleCursor;
            if (m_shuffleCursor >= m_shuffle.size()) m_shuffleCursor = 0;
        }
    }
}

void PlaylistViewModel::clear() {
    if (m_tracks.isEmpty() && m_currentIndex < 0) return;
    m_tracks.clear();
    m_shuffle.clear();
    m_shuffleCursor = 0;
    if (m_currentIndex != -1) {
        m_currentIndex = -1;
        emit currentIndexChanged(-1);
    }
    emit cleared();
}

void PlaylistViewModel::setCurrentIndex(int idx, bool emitRequest) {
    if (idx < 0 || idx >= m_tracks.size()) return;
    if (idx != m_currentIndex) {
        m_currentIndex = idx;
        emit currentIndexChanged(idx);
    }
    if (emitRequest) {
        emit currentTrackRequested(m_tracks[idx].path);
    }
}

void PlaylistViewModel::setCurrentByPath(const QString& path, bool emitRequest) {
    int idx = indexOf(path);
    if (idx >= 0) setCurrentIndex(idx, emitRequest);
}

void PlaylistViewModel::next() {
    if (m_tracks.isEmpty()) return;
    int n = m_currentIndex + 1;
    if (n >= m_tracks.size()) n = 0;
    setCurrentIndex(n, /*emitRequest=*/true);
}

void PlaylistViewModel::previous() {
    if (m_tracks.isEmpty()) return;
    int p = m_currentIndex - 1;
    if (p < 0) p = m_tracks.size() - 1;
    setCurrentIndex(p, /*emitRequest=*/true);
}

void PlaylistViewModel::advanceForFinish() {
    if (m_tracks.isEmpty()) return;
    switch (m_playMode) {
    case PlayMode::ListLoop: {
        int n = m_currentIndex + 1;
        if (n >= m_tracks.size()) n = 0;
        setCurrentIndex(n, true);
        break;
    }
    case PlayMode::SingleRepeat:
        // 同一首再来一遍：索引不变，但需要让 View 重新触发播放
        if (m_currentIndex >= 0) {
            emit currentTrackRequested(m_tracks[m_currentIndex].path);
        }
        break;
    case PlayMode::Shuffle: {
        if (m_shuffle.isEmpty()) rebuildShuffleIfNeeded();
        if (m_shuffle.isEmpty()) return;
        ++m_shuffleCursor;
        if (m_shuffleCursor >= m_shuffle.size()) {
            m_shuffleCursor = 0;
            std::shuffle(m_shuffle.begin(), m_shuffle.end(),
                         std::mt19937(std::random_device{}()));
        }
        int idx = indexOf(m_shuffle.at(m_shuffleCursor));
        if (idx >= 0) setCurrentIndex(idx, true);
        break;
    }
    }
}

void PlaylistViewModel::togglePlayMode() {
    switch (m_playMode) {
    case PlayMode::ListLoop:     setPlayMode(PlayMode::SingleRepeat); break;
    case PlayMode::SingleRepeat: setPlayMode(PlayMode::Shuffle);      break;
    case PlayMode::Shuffle:      setPlayMode(PlayMode::ListLoop);     break;
    }
}

void PlaylistViewModel::setPlayMode(PlayMode mode) {
    if (mode == m_playMode) return;
    m_playMode = mode;
    if (mode == PlayMode::Shuffle) {
        rebuildShuffleIfNeeded();
    } else {
        m_shuffle.clear();
        m_shuffleCursor = 0;
    }
    emit playModeChanged(mode);
}

void PlaylistViewModel::rebuildShuffleIfNeeded() {
    m_shuffle.clear();
    m_shuffle.reserve(m_tracks.size());
    for (const Track& t : m_tracks) m_shuffle.append(t.path);
    std::shuffle(m_shuffle.begin(), m_shuffle.end(),
                 std::mt19937(std::random_device{}()));
    m_shuffleCursor = 0;
}

// ============== 持久化 ==============
void PlaylistViewModel::loadFromConfig() {
    ConfigManager& cfg = ConfigManager::instance();

    m_tracks.clear();
    const QList<ConfigManager::VideoItem> items = cfg.getVideoList();
    m_tracks.reserve(items.size());
    for (const ConfigManager::VideoItem& it : items) {
        Track t;
        t.path          = it.path;
        t.name          = QFileInfo(it.path).fileName();
        t.durationSec   = it.duration;
        t.thumbnailPath = it.thumbnail;
        m_tracks.append(t);
    }
    emit tracksReset();

    // 当前索引
    int savedIndex = cfg.getCurrentIndex();
    if (savedIndex < 0 || savedIndex >= m_tracks.size()) {
        savedIndex = m_tracks.isEmpty() ? -1 : 0;
    }
    if (savedIndex != m_currentIndex) {
        m_currentIndex = savedIndex;
        emit currentIndexChanged(savedIndex);
    }

    // 播放模式
    PlayMode mode = cfg.getPlayMode();
    if (mode != m_playMode) {
        m_playMode = mode;
        if (mode == PlayMode::Shuffle) rebuildShuffleIfNeeded();
        emit playModeChanged(mode);
    } else if (mode == PlayMode::Shuffle) {
        rebuildShuffleIfNeeded();
    }
}

void PlaylistViewModel::writeToConfig() const {
    ConfigManager& cfg = ConfigManager::instance();

    QList<ConfigManager::VideoItem> items;
    items.reserve(m_tracks.size());
    for (const Track& t : m_tracks) {
        ConfigManager::VideoItem it;
        it.path      = t.path;
        it.name      = t.name.isEmpty() ? QFileInfo(t.path).fileName() : t.name;
        it.duration  = t.durationSec;
        it.thumbnail = t.thumbnailPath.isEmpty() ? cfg.thumbnailPathForVideo(t.path) : t.thumbnailPath;
        items.append(it);
    }
    cfg.setVideoList(items);
    cfg.setCurrentIndex(m_currentIndex);
    cfg.setPlayMode(m_playMode);
}
