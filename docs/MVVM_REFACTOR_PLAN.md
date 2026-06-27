# Smart_Player MVVM 重构设计方案

> 目标读者：可被 AI 直接按"模块 → 阶段"顺序执行的任务清单。
> 适用项目：`d:/lyj/projects/Smart_Player`（Qt 6.9 + MinGW，`.pro` 自动收集源码）。
>
> 文档约定：
> - 每阶段都明确**输入产物 / 改动点 / 验收回归用例**，AI 执行完一个阶段即可交付。
> - 标 ✅ 的阶段已经完成；标 ⏳ 的待执行。
> - **AI 执行规则**：必须严格遵守"阶段顺序"，每完成一个阶段必须运行《静态检查 + 自检清单》后再进入下一阶段。

---

## 0. 总体架构图（重构后）

```
┌───────────────────────────────────────────────────────────────┐
│                         View 层 (QWidget)                      │
│  MainWindow / SettingDialog / SummarySettingsDialog            │
│  SummaryPanel(View) / TranscriptPanel(View)                    │
│  VideoInfoDialog / SubtitlePopup / VideoSlider / OpenGLRenderer│
│  ─ 只做：UI 构建 / 事件转发 / 渲染响应 VM signal                 │
└──────────────────┬────────────────────────────────────────────┘
                   │  connect(vm, &Vm::xxxChanged, ...)
                   │  vm->command()
                   ▼
┌───────────────────────────────────────────────────────────────┐
│                       ViewModel 层 (src/viewmodel/)              │
│  PlayerViewModel      ✅  PlaylistViewModel    ✅              │
│  SummaryViewModel     ⏳  TranscriptViewModel  ⏳              │
│  SettingsViewModel    ⏳  AppShellViewModel    ⏳ (可选)        │
│  ─ Q_PROPERTY + slot + signal；不依赖任何 ui_*.h                │
└──────────────────┬────────────────────────────────────────────┘
                   │  持有 / 调用
                   ▼
┌───────────────────────────────────────────────────────────────┐
│                          Model 层                              │
│  src/core/PlayerCore   src/summary/VideoSummaryManager                 │
│  src/subtitle/AsrManager   translator/SubtitleTranslator           │
│  src/demuxer/decoder/render/queue/...  src/app/ConfigManager           │
│  ─ 纯业务、纯数据、无 UI 依赖                                   │
└───────────────────────────────────────────────────────────────┘
```

### 不变量（所有 AI 后续修改必须遵守）

1. **src/viewmodel/ 目录禁止 `#include` 任何 `ui_xxx.h` 与 `QWidget` 派生类的私有头**。
2. **Model（src/core/ src/summary/ src/subtitle/ translator/ src/demuxer/ ...）禁止 `#include` 任何 `src/viewmodel/` 或 View 头**。
3. **View 禁止直接调用 Model 方法**（PlayerCore::play、VideoSummaryManager::startSummary 等），必须通过 VM。例外：`PlayerViewModel::core()` / `avFormatContext()` 等少量"逃生口"，新代码禁止使用。
4. **每个 VM 字段必须有 `Q_PROPERTY` 或显式 signal 通知**。无 NOTIFY 的状态变更视为 bug。
5. **持久化**：VM 不直接读写文件系统；通过 `ConfigManager` 进行（`SettingsViewModel` 阶段后 ConfigManager 退化为存储底层，由 SettingsViewModel 包装）。
6. **线程**：VM 都生活在 GUI 线程；与 Model 的跨线程交互沿用 Qt 队列连接。

---

## 1. 已完成（参考实现，新增 VM 模板）

### ✅ 阶段 1：PlayerViewModel

- **新增文件**：`src/viewmodel/iviewmodel.h`、`src/viewmodel/playerviewmodel.{h,cpp}`、`src/viewmodel/README.md`
- **改动文件**：`src/app/mainwindow.{h,cpp}`、`Smart_Player.pro`
- **核心做法**：
  - `PlayerCore` 被 VM 拥有；VM 通过 Q_PROPERTY 暴露 `state / position / duration / volume / mute / speedIndex / asrEnabled / hasAudio / hasVideo / fileUrl / currentSubtitle`。
  - VM 信号刻意与 PlayerCore 同名同签名，便于 View 平滑迁移。
- **后续 AI 引用此模式**：所有 VM 都按"`Q_PROPERTY` + 同名 NOTIFY signal + slot 命令"三件套来。

### ✅ 阶段 2：PlaylistViewModel

- **新增文件**：`src/viewmodel/playlistviewmodel.{h,cpp}`
- **改动文件**：`src/app/mainwindow.{h,cpp}`
- **核心做法**：
  - 数据 `tracks / currentIndex / playMode` 全部迁出 MainWindow。
  - `currentTrackRequested(path)` 信号承担"用户意图：播放某条"，View 接到后调 `play(path)`。VM 与 PlayerViewModel **不直接交互**。
- **后续 AI 引用此模式**：跨 VM 协作禁止 VM-to-VM 直调，改为 VM 发"意图信号"由 Controller（MainWindow）做协调。

---

## 2. 待执行阶段（按顺序，每个阶段相对独立）

### ⏳ 阶段 3a：SummaryViewModel —— 包装 VideoSummaryManager

#### 目标
- 把 `SummaryPanel` 对 `VideoSummaryManager` 的直接耦合（`bindManager(m_summaryManager)`）切断。
- `SummaryPanel` 只与 `SummaryViewModel` 对话。
- `VideoSummaryManager` 一行不动（它已经是高质量 Model）。

#### 新增文件
1. `src/viewmodel/summaryviewmodel.h`
2. `src/viewmodel/summaryviewmodel.cpp`

#### SummaryViewModel 设计骨架

```cpp
// src/viewmodel/summaryviewmodel.h
class SummaryViewModel : public IViewModel {
    Q_OBJECT
    Q_PROPERTY(SummaryState state                  READ state                NOTIFY stateChanged)
    Q_PROPERTY(double       overallProgress        READ overallProgress      NOTIFY progressChanged)
    Q_PROPERTY(double       stageProgress          READ stageProgress        NOTIFY progressChanged)
    Q_PROPERTY(int          currentSegment         READ currentSegment       NOTIFY progressChanged)
    Q_PROPERTY(int          totalSegments          READ totalSegments        NOTIFY progressChanged)
    Q_PROPERTY(QString      videoPath              READ videoPath            NOTIFY videoPathChanged)
    Q_PROPERTY(QString      lastErrorMessage       READ lastErrorMessage     NOTIFY errorOccurred)
    Q_PROPERTY(bool         hasReport              READ hasReport            NOTIFY reportChanged)

public:
    explicit SummaryViewModel(QObject* parent = nullptr);

    // 状态 getter（透传到 manager）
    SummaryState state() const;
    double overallProgress() const;
    double stageProgress()   const;
    int    currentSegment()  const;
    int    totalSegments()   const;
    QString videoPath() const { return m_videoPath; }
    QString lastErrorMessage() const { return m_lastError; }
    bool   hasReport() const { return m_report.isValid; }

    // 数据访问（只读视图，View 在 reportChanged 后拉一次）
    const SummaryReport&        report()        const { return m_report; }
    const QList<SummarySegment>& segments()      const;   // 透传 manager
    const QList<SubtitleItem>&  asrResults()    const;   // 透传 manager

    // 缓存相关（透传）
    bool tryLoadFromCache(const QString& videoPath);
    void saveToCache();
    static void   clearAllCache();
    static qint64 cacheTotalSize();
    static int    cacheFileCount();

public slots:
    // 命令
    void setVideoPath(const QString& path);   // 路径变化时自动 reset
    void start();                              // 等价 manager.startSummary(m_videoPath)
    void stop();
    void rerun();                              // stop + start
    void exportMarkdownTo(const QString& filePath);  // 导出（VM 不直接写盘，调 ConfigManager / QFile）
    void setModel(const QString& model);

signals:
    void stateChanged(SummaryState state);
    void progressChanged();              // 包含 overall/stage/cur/total，View 用 getter 一次性拉
    void segmentAnalyzed(int index, const QString& description);
    void videoPathChanged(const QString& path);
    void errorOccurred(const QString& message);
    void reportChanged();                // 收到 structuredReportReady 后整体更新

    // 用户意图：seek 到某 ms（章节点击 / 实体点击）
    void seekRequested(qint64 ms);

private slots:
    void onMgrStateChanged(SummaryState s);
    void onMgrProgressDetail(const VideoSummaryManager::Progress& p);
    void onMgrSegmentAnalyzed(int index, const QString& description);
    void onMgrStructuredReportReady(const SummaryReport& report);
    void onMgrError(const QString& msg);

private:
    VideoSummaryManager* m_manager = nullptr;   // VM 拥有
    QString              m_videoPath;
    SummaryReport        m_report;              // 缓存最近一次完整 report
    QString              m_lastError;
};
```

#### 改动 MainWindow

```cpp
// 旧：
m_summaryManager = new VideoSummaryManager(this);
m_summaryPanel->bindManager(m_summaryManager);

// 新：
m_summaryVm = new SummaryViewModel(this);
m_summaryPanel->bindViewModel(m_summaryVm);   // 见阶段 3b
```

#### 改动 SummaryPanel
- `bindManager(VideoSummaryManager*)` → 改名 `bindViewModel(SummaryViewModel*)`。
- 内部所有 `m_manager->xxx()` 改为 `m_vm->xxx()`，所有 `connect(m_manager, ...)` 改为 `connect(m_vm, ...)`。
- 由于 VM 已和 Manager API 同名同 signal，SummaryPanel 本身改动 < 20 行。

#### 验收回归
- 启动总结 / 中断停止 / 重新生成 / 导出 / 切换模型 / 切视频自动重置
- 缓存命中场景：打开同一视频应立即从缓存还原
- 错误场景：网络断开时 errorOccurred 是否正常显示

---

### ⏳ 阶段 3b：TranscriptViewModel —— TranscriptPanel 拆解（最大的一刀）

> ⚠️ 这是整个重构最复杂的一阶段。`transcriptpanel.cpp` 有 1317 行。
> 建议 AI 严格按以下子任务顺序执行。

#### 目标
- `TranscriptPanel`（QWidget）只负责：QListWidget 渲染、ItemDelegate 绘制、用户事件转发、滚动行为。
- `TranscriptViewModel` 负责：`m_rows / m_subtitles / m_chapters / m_segments / m_lineCache / 高亮当前段/句/字 / 搜索过滤 / 折叠状态 / 自动滚动状态机`。

#### 新增文件
1. `src/viewmodel/transcriptviewmodel.h`
2. `src/viewmodel/transcriptviewmodel.cpp`

#### 数据归属切分（按 TranscriptPanel 字段逐个分类）

| 字段 | 归属 | 说明 |
|---|---|---|
| `m_subtitles / m_chapters / m_segments` | **VM** | 业务数据，UI 无关 |
| `m_rows / m_lineCache` | **VM** | 业务计算缓存 |
| `m_activeSubtitleIdx / m_activeChapterIdx / m_currentPosMs` | **VM** | 跟随播放位置的状态 |
| `m_searchKeyword / m_searchHitRows` | **VM** | 业务过滤状态 |
| `m_wordLevelEnabled / m_autoScroll` | **VM** | 用户偏好；可观察 |
| `m_hasChapters / m_hasSubtitles / m_hasSegments` | **VM** | 派生状态（hasReport 类） |
| `m_collapsed[chapter]` | **VM** | 业务折叠状态（每章独立） |
| `m_hoverSubtitleIdx` | **View** | 纯 UI 交互态（鼠标悬浮） |
| `m_hitRects` | **View** | 绘制坐标缓存，每次 paint 重建 |
| `m_list / m_search / m_btnXxx / m_throttleTimer / m_autoScrollResumeTimer` | **View** | UI 控件 |
| `m_emptyLabel / m_totalDurationMs` | **View 或 VM** | `m_totalDurationMs` 建议入 VM（formatTime 需要） |

#### TranscriptViewModel 设计骨架

```cpp
class TranscriptViewModel : public IViewModel {
    Q_OBJECT
    Q_PROPERTY(QString  videoPath          READ videoPath          NOTIFY videoPathChanged)
    Q_PROPERTY(qint64   durationMs         READ durationMs         NOTIFY durationChanged)
    Q_PROPERTY(qint64   currentPositionMs  READ currentPositionMs  NOTIFY positionChanged)
    Q_PROPERTY(int      activeSubtitleIdx  READ activeSubtitleIdx  NOTIFY activeIndexChanged)
    Q_PROPERTY(int      activeChapterIdx   READ activeChapterIdx   NOTIFY activeIndexChanged)
    Q_PROPERTY(bool     wordLevelEnabled   READ wordLevelEnabled   WRITE setWordLevelEnabled NOTIFY wordLevelToggled)
    Q_PROPERTY(QString  searchKeyword      READ searchKeyword      WRITE setSearchKeyword    NOTIFY searchChanged)
    Q_PROPERTY(bool     autoScroll         READ autoScroll         WRITE setAutoScroll       NOTIFY autoScrollChanged)
    Q_PROPERTY(bool     hasChapters        READ hasChapters        NOTIFY dataReady)
    Q_PROPERTY(bool     hasSubtitles       READ hasSubtitles       NOTIFY dataReady)
    Q_PROPERTY(bool     hasSegments        READ hasSegments        NOTIFY dataReady)

public:
    // 数据访问（只读）
    const QList<RowItem>&    rows() const;
    const QList<SubtitleItem>& subtitles() const;
    const QList<SummaryChapter>& chapters() const;
    QHash<int, SubtitleLineCache*> lineCache() const;
    bool isSearchHit(int subtitleIndex) const;
    bool isCollapsed(int chapterIndex) const;
    int activeWordInLine(int subtitleIndex, qint64 posMs) const;
    QString tooltipForSubtitle(int subtitleIndex) const;
    QString formatTime(qint64 ms) const;

    int findActiveSubtitleIndex(qint64 posMs) const;
    int findRowIndexBySubtitle(int subtitleIndex) const;
    int findRowIndexByChapter(int chapterIndex) const;

public slots:
    // 数据注入
    void setVideoPath(const QString& path);
    void setSubtitles(const QList<SubtitleItem>& items);
    void setChapters(const QList<SummaryChapter>& chapters);
    void setSegments(const QList<SummarySegment>& segments);
    void setDuration(qint64 ms);
    void clearAll();

    // 状态控制
    void updatePosition(qint64 ms);          // 节流由 View 控制
    void setWordLevelEnabled(bool on);
    void setSearchKeyword(const QString& kw);
    void setAutoScroll(bool on);

    void toggleChapterCollapsed(int chapterIndex);
    void expandAll();
    void collapseAll();
    void collapseAllFinishedChapters();      // 等价旧 onCollapseFinishedClicked

    // 用户意图
    void requestSeekToSubtitle(int subtitleIndex);
    void requestSeekToChapter(int chapterIndex);

signals:
    // 状态信号
    void videoPathChanged(const QString& p);
    void durationChanged(qint64 ms);
    void positionChanged(qint64 ms);
    void activeIndexChanged();               // subtitle/chapter 任一变化
    void wordLevelToggled(bool on);
    void searchChanged(const QString& kw);
    void autoScrollChanged(bool on);

    // 结构变化
    void rowsRebuilt();                      // m_rows 重建完成，View 重新绑定到 QListWidget
    void rowsCollapseChanged();              // 折叠状态变化，View 调 applyCollapseToList
    void dataReady();                        // hasChapters/Subtitles/Segments 任一变化

    // 用户意图
    void seekTo(qint64 ms);

private:
    void rebuildRows();
    void ensureLineCache(int subtitleIndex);
    SubtitleLineCache buildLineCache(int subtitleIndex) const;
    static QList<QPair<int,int>> tokenize(const QString& text);

    // 状态字段（与旧 TranscriptPanel 字段一一映射）
    QList<RowItem>            m_rows;
    QList<SubtitleItem>       m_subtitles;
    QList<SummaryChapter>     m_chapters;
    QList<SummarySegment>     m_segments;
    QHash<int, SubtitleLineCache*> m_lineCache;
    QSet<int>                 m_searchHitRows;
    QSet<int>                 m_collapsedChapters;
    QString                   m_videoPath;
    QString                   m_searchKeyword;
    qint64                    m_durationMs   = 0;
    qint64                    m_currentPosMs = -1;
    int                       m_activeSubtitleIdx = -1;
    int                       m_activeChapterIdx  = -1;
    bool                      m_wordLevelEnabled  = true;
    bool                      m_autoScroll        = true;
};
```

#### TranscriptPanel 改造要点

- 删除：所有 `m_subtitles / m_chapters / m_rows / m_lineCache / m_searchHitRows / m_active* / m_wordLevelEnabled / m_searchKeyword / m_autoScroll / m_currentPosMs / m_totalDurationMs / m_has*` 字段。
- 保留：`m_list / m_search / m_btn* / m_chkWordLevel / m_throttleTimer / m_autoScrollResumeTimer / m_emptyLabel / m_hoverSubtitleIdx / m_hitRects`。
- 增加：`TranscriptViewModel* m_vm`。
- `TranscriptItemDelegate` 改为通过 `m_panel->vm()->rows() / lineCache() / activeXxx()` 取数据。
- 节流定时器（`m_throttleTimer`）仍由 View 持有，超时时调用 `m_vm->updatePosition(latestMs)`。
- 自动滚动：`m_autoScrollResumeTimer` 仍由 View 持有，超时调 `m_vm->setAutoScroll(true)`；用户主动滚动时 View 调 `m_vm->setAutoScroll(false)`。
- 搜索：用户输入 → `m_vm->setSearchKeyword(kw)`；VM 触发 `searchChanged` → View 强制 `m_list->viewport()->update()`。
- 折叠：用户点章节头三角 → `m_vm->toggleChapterCollapsed(idx)`；VM 触发 `rowsCollapseChanged` → View 调 `applyCollapseToList()`。

#### MainWindow 改动

```cpp
// 旧：
m_transcriptPanel = new TranscriptPanel();
connect(m_summaryManager, &VideoSummaryManager::asrCompleted,
        m_transcriptPanel, &TranscriptPanel::setSubtitleItems);
connect(player_, &PlayerViewModel::timeChanged, m_transcriptPanel, [this]() {
    m_transcriptPanel->onPositionChanged(player_->currentPos());
});
connect(player_, &PlayerViewModel::initFinished, this, [this]() {
    m_transcriptPanel->setDuration(player_->duration());
});

// 新：
m_transcriptVm = new TranscriptViewModel(this);
m_transcriptPanel = new TranscriptPanel();
m_transcriptPanel->bindViewModel(m_transcriptVm);

// VM ↔ VM 数据流由 MainWindow 协调（不让 VM 之间直接 connect）
connect(m_summaryVm, &SummaryViewModel::reportChanged, this, [this]() {
    m_transcriptVm->setChapters(m_summaryVm->report().chapters);
    m_transcriptVm->setSegments(m_summaryVm->report().segments);
    m_transcriptVm->setSubtitles(m_summaryVm->report().asrResults);   // 缓存场景
});
connect(m_summaryVm, &SummaryViewModel::asrCompleted /* 透传 */, ...);
connect(player_, &PlayerViewModel::positionChanged, m_transcriptVm, &TranscriptViewModel::updatePosition);
connect(player_, &PlayerViewModel::durationChanged, m_transcriptVm, &TranscriptViewModel::setDuration);

// 用户意图回路
connect(m_transcriptVm, &TranscriptViewModel::seekTo, this, [this](qint64 ms) {
    player_->seek(ms * 1000);   // ms → μs
});
```

#### 子任务拆分（建议 AI 按顺序执行）

1. **3b-1**：创建 `TranscriptViewModel` 骨架，把字段、构造、`Q_PROPERTY`、信号声明都加上；**实现先全部 stub**（直接返回成员、不做计算）。
2. **3b-2**：把 `rebuildRows()` / `buildLineCache()` / `findActiveSubtitleIndex()` / `findRowIndexBySubtitle()` 等纯算法函数从 `TranscriptPanel` 整体平移到 `TranscriptViewModel`，签名不变。
3. **3b-3**：把数据 setter（setSubtitles/Chapters/Segments/Duration/clearAll）也搬过去；在 setter 末尾 emit `dataReady` / `rowsRebuilt`。
4. **3b-4**：把"高亮当前位置"逻辑（`applyHighlight` / `findActiveSubtitleIndex` / `activeWordInLine`）搬过去；保留旧 `applyHighlight` 接口但实现改为：算完 activeXxx → emit `activeIndexChanged`。
5. **3b-5**：折叠状态改造 —— 把 `RowItem::collapsed` 字段废弃（保留也行，但权威值在 `m_collapsedChapters`）。`toggleChapterCollapsed` 由 VM 提供。
6. **3b-6**：搜索状态改造 —— `setSearchKeyword` 触发重新计算 `m_searchHitRows`。
7. **3b-7**：`TranscriptPanel` 改造为 View —— 字段瘦身、删旧实现、改为 `m_vm->xxx()` 转发、connect VM 信号到 View 渲染。
8. **3b-8**：`TranscriptItemDelegate` 透过 `m_panel->vm()` 取数据（或让 Delegate 直接持有 `m_vm`，更干净）。
9. **3b-9**：MainWindow 切换；删除原有 panel-manager-player 之间的 connect，改为 VM-VM 经 MainWindow 协调。
10. **3b-10**：编译 + 自检。

#### 验收回归
- 启动 AI 总结后字幕/章节出现的时机与旧版一致
- 播放过程当前句变蓝、自动滚动跟随
- 用户拖动滚动条 N 秒内不自动滚动，N 秒后恢复
- 搜索关键字命中高亮
- 折叠/展开全部/折叠已完成章节
- 双击章节头 / 段落 seek 到位
- 切换字级模式 / 切换视频

---

### ⏳ 阶段 4：SettingsViewModel —— 统一配置入口

#### 目标
- 把 `ConfigManager` 的所有 setter/getter 包装成 VM 风格的 `Q_PROPERTY`，让 `settingDialog` / `SummarySettingsDialog` / `MainWindow::applyPersistentSettings()` 都从 VM 读写。
- `ConfigManager` 沦为单纯的"存储后端"。

#### 新增文件
1. `src/viewmodel/settingsviewmodel.h`
2. `src/viewmodel/settingsviewmodel.cpp`

#### SettingsViewModel 设计骨架

```cpp
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
    static SettingsViewModel& instance();   // 全局单例，与 ConfigManager 对齐

    // ... 全部 getter/setter ...

public slots:
    void loadFromDisk();   // 启动期调用一次
    void saveToDisk();     // 用户点确认后调用

signals:
    void hardwareChanged(bool v);
    // ... 每个属性一个 NOTIFY ...
    void summaryConfigChanged();
    void anyChanged();      // 任何属性变化都 emit（方便 dialog 启用"应用"按钮）
};
```

#### 改动列表

1. **`settingDialog`**：
   - 删除所有 `originalXxx_` 字段（VM 内部保留旧值快照）。
   - 构造时拿 `SettingsViewModel::instance()` 并 connect 到对应控件。
   - 控件变化 → `vm.setXxx(value)`，VM 内部统一 emit；取消按钮 → `vm.revert()`；确认按钮 → `vm.commit()` + `vm.saveToDisk()`。
2. **`SummarySettingsDialog`**：同上策略。
3. **`MainWindow`**：
   - 旧 `connect(settingdialog, &settingDialog::startHardWareAccep, this, &MainWindow::on_setHardWare)` 改为 `connect(&settingsVm, &SettingsViewModel::hardwareChanged, this, &MainWindow::on_setHardWare)`。
   - `applyPersistentSettings()` 直接读 `settingsVm.volume()` 等。

#### 验收回归
- 设置对话框打开 → 取消 → 不持久化
- 设置对话框打开 → 确认 → 持久化
- 启动后立即应用上次设置（解码方式、亮度、对比度、模型路径等）
- AI 总结设置对话框联动 `SummaryViewModel.setModel()` 等

---

### ⏳ 阶段 5：MainWindow 瘦身 + 收尾

#### 目标
- `MainWindow` 仅剩：UI 组装 / 全屏切换 / 快捷键 / 鼠标事件 / 控制栏自动隐藏 / dock 切换 / 4 个 VM 实例之间的协调 connect。
- 行数 < 500（当前 1299）。

#### 子任务

1. **抽 `ControlBarController`（非必须，可选）**：把控制栏自动隐藏、全屏切换、鼠标事件相关逻辑提到独立类。
2. **删除 `VideoInfoDialog::updateinformation(AVFormatContext*, ...)` 的 AVFormatContext 依赖**：
   - 在 `PlayerViewModel` 新增 `Q_PROPERTY(QVariantMap mediaInfo READ mediaInfo NOTIFY mediaInfoReady)` 或 DTO `struct MediaInfo`。
   - `VideoInfoDialog` 接受 `const MediaInfo&` 而非原生 AVFormatContext。
   - 删除 `PlayerViewModel::avFormatContext()` 逃生口。
3. **删除 `PlayerViewModel::core()` 逃生口**：搜索 `vm->core()`、`->core()->`，逐处用 VM API 替换。
4. **`SubtitlePopup` 与 `videoWidget->setSubtitleFontSize`**：把 fontSize 状态接到 `SettingsViewModel`。
5. **将 `m_summaryVm / m_transcriptVm` 之间的协调挪到一个 `AppShellViewModel`（可选）**：如果协调逻辑足够多。

#### 验收回归
- 全功能回归：打开、播放、暂停、停止、seek、音量、倍速、截图、字幕、设置、AI 总结、文稿、全屏、控制栏自动隐藏、快捷键。

---

## 3. 跨阶段通用规范

### 3.1 VM 文件骨架模板

每个新增 VM 都应包含：

```cpp
// src/viewmodel/xxxviewmodel.h
#ifndef XXXVIEWMODEL_H
#define XXXVIEWMODEL_H

#include "iviewmodel.h"

class XxxViewModel : public IViewModel {
    Q_OBJECT
    Q_PROPERTY(...)

public:
    explicit XxxViewModel(QObject* parent = nullptr);
    ~XxxViewModel() override = default;

    // getters

public slots:
    // commands

signals:
    // notify / events / intents

private:
    // state
};

#endif
```

### 3.2 信号命名约定

- **属性变更通知**：`xxxChanged` 或与 PlayerCore 兼容的同名。
- **业务事件**：`xxxOccurred` / `xxxReady` / `xxxFinished`。
- **用户意图**：`xxxRequested`（如 `seekRequested`、`currentTrackRequested`）。

### 3.3 跨 VM 协作规则

- VM 之间**绝对禁止**直接持有对方指针或 connect 对方信号。
- 协调放在 **Controller**（当前阶段是 MainWindow，未来可抽 AppShellViewModel）。
- 单向"意图信号" → Controller 转发为对应 VM 的命令。

### 3.4 .pro 文件维护

无需手动编辑 SOURCES/HEADERS（已使用 `$$files()` 自动收集）。新增目录时只在 `SRC_DIRS` 里加一行即可。

### 3.5 编译验证

每次阶段结束后必须执行：
1. `read_lints` 检查所有改动文件
2. 启动 Qt Creator → 执行 qmake → Build
3. 跑该阶段的"验收回归"用例

### 3.6 调试技巧

- VM 调试可以用 Qt 的 `QSignalSpy` 写单元测试，验证"输入命令 → 期望信号"。
- 建议每个 VM 都加 `qDebug() << "Vm::xxx" << ...` 日志，方便定位。

---

## 4. AI 执行清单（按顺序）

| 阶段 | 状态 | 触发条件 | 输出 |
|---|---|---|---|
| 1. PlayerViewModel | ✅ 已完成 | - | src/viewmodel/{iviewmodel,playerviewmodel}.{h,cpp} |
| 2. PlaylistViewModel | ✅ 已完成 | - | src/viewmodel/playlistviewmodel.{h,cpp} |
| **3a. SummaryViewModel** | ⏳ 待执行 | 用户说"执行阶段 3a" | src/viewmodel/summaryviewmodel.{h,cpp} + SummaryPanel/MainWindow 改造 |
| **3b. TranscriptViewModel** | ⏳ 待执行 | 用户说"执行阶段 3b" | src/viewmodel/transcriptviewmodel.{h,cpp} + TranscriptPanel/MainWindow 改造 |
| **4. SettingsViewModel** | ⏳ 待执行 | 用户说"执行阶段 4" | src/viewmodel/settingsviewmodel.{h,cpp} + settingDialog/SummarySettingsDialog/MainWindow 改造 |
| **5. MainWindow 瘦身 + 收尾** | ⏳ 待执行 | 用户说"执行阶段 5" | MainWindow < 500 行；删除 VM 逃生口 |

### AI 执行规则

1. 每次只执行一个阶段，完成后总结改动并等待用户验收。
2. 阶段内子任务按顺序执行，**不允许跳过**子任务。
3. 修改文件时必须先 `read_file` 最新内容；不要基于"我已经记得"做修改。
4. 中文注释保留；编码 UTF-8 无 BOM（Windows + qmake + g++）。
5. 阶段开始前必须先确认依赖阶段已完成（看本文档表格的 ✅ 标记）。
6. 静态检查（read_lints）必须通过；编译由用户在 Qt Creator 中执行。

---

## 5. 不在重构范围的事项（明确说不做）

- 不重写底层 FFmpeg 解码栈（src/demuxer/decoder/converter/queue 等）。
- 不引入 QML（保留 QWidget；MVVM 设计也是为未来 QML 留余地）。
- 不引入第三方 MVVM 框架（KO / Vue 风格）；直接用 Qt 原生 Q_PROPERTY + signal/slot。
- 不修改 `Resource.qrc / app_icon.rc / SmartPlayer-icon/`。
- 不重写 `ConfigManager` 内部存储格式（QSettings + JSON 双轨保留）。
- 不动 ASR / 翻译两个模块的 Model 层（仅未来给它们也包 VM 时再说）。

---

## 6. 验收里程碑

| 里程碑 | 完成标志 |
|---|---|
| M1 | `MainWindow` 不再 `#include "playercore.h"`，仅 include `src/viewmodel/playerviewmodel.h`（✅ 已达成） |
| M2 | `MainWindow` 不再持有 `fileList / listIndex / playMode_`（✅ 已达成） |
| M3 | `SummaryPanel` 不再 `#include "videosummarymanager.h"` | 阶段 3a 完成 |
| M4 | `TranscriptPanel.cpp` < 600 行 | 阶段 3b 完成 |
| M5 | `settingDialog` / `SummarySettingsDialog` 不再 `#include "configmanager.h"` | 阶段 4 完成 |
| M6 | `MainWindow.cpp` < 500 行 | 阶段 5 完成 |
| M7 | `PlayerViewModel::core() / avFormatContext()` 被全部删除 | 阶段 5 完成 |

---

## 7. 风险与对策

| 风险 | 影响 | 对策 |
|---|---|---|
| `TranscriptPanel` 拆解时 ItemDelegate 与 VM 之间的访问关系处理不当 | 高 | 让 Delegate 直接持有 VM 指针，不通过 Panel 间接访问 |
| 跨 VM 协作的循环触发（A 改 → emit → B 改 → emit → A 改）| 中 | 所有 setter 内做"值未变直接 return"；用 QSignalBlocker 防回路 |
| `ConfigManager` 单例时序与 `SettingsViewModel` 单例时序冲突 | 中 | `SettingsViewModel` 首次访问时主动 `ConfigManager::instance().load()` |
| 视频帧透传 signal 的拷贝开销 | 低 | 现状已是 QByteArray 隐式共享，VM 不做任何处理；保持现状 |
| MOC 对 `using` 别名 + Q_PROPERTY 的支持差异 | 低 | 已规避（PlayerViewModel 用 `enum State` 而非 `enum class` + alias） |
