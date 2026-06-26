# Smart Player

基于 Qt + FFmpeg 的智能多媒体播放器，支持 AI 字幕、AI 视频总结、视频文稿等高级功能。

## 项目架构

项目采用 **MVVM（Model-View-ViewModel）** 架构，主要模块如下：

| 模块目录 | 功能说明 |
|---------|---------|
| `app/` | UI 层（MainWindow、控件、弹窗等） |
| `viewmodel/` | ViewModel 层（PlayerViewModel、PlaylistViewModel、SummaryViewModel 等） |
| `core/` | 播放核心（PlayerCore，解复用/解码/渲染调度） |
| `demuxer/` | 解复用器（FFmpeg AVFormatContext 封装） |
| `decoder/` | 解码器（支持硬件加速） |
| `render/` | 渲染层（OpenGL 视频渲染、SDL 音频输出） |
| `queue/` | 音视频帧/包队列（线程安全） |
| `filter/` | 音视频滤镜 |
| `converter/` | 格式转换 |
| `resampler/` | 音频重采样 |
| `subtitle/` | 字幕模块（基于 whisper.cpp 的 ASR 实时字幕） |
| `summary/` | AI 视频总结（智能语义分段 + VLM 场景分类） |
| `translator/` | 翻译模块 |
| `pool/` | 线程池 |
| `utils/` | 工具类（日志、截图等） |

## Git 提交历史

| 提交 | 说明 |
|------|------|
| `dfaef3a` | Init Commit |
| `570884e` | 代码重构 |
| `862052d` | 添加基于 whisper.cpp 的 AI 字幕功能 |
| `54df8d2` | 新增列表模式切换，修复一些问题 |
| `ef366c7` | 新增 AI 总结面板 |
| `9f105a0` | 新增 AI 智能语义分段 + VLM 批量场景分类 |
| `436d38e` | AI 总结分析结果缓存 |
| `0d0b641` | AI 总结设置项迁移 |
| `40183d1` | 移除字幕显示模块，修改 UI |
| `649406c` | 新增视频文稿面板 |
| `a1b9210` | 调整了目录结构 |
| `f8fbef1` | 修复了 seek 后卡顿的问题 |
| `f86bcbc` | MVVM 架构重构 |
| `f88aeaf` | 完成剩余 MVVM 重构部分 |

## 本次修改内容（未提交）

基于最新提交 `f88aeaf`（完成剩余 MVVM 重构部分），本次修改涵盖 **Bug 修复、资源泄漏修复、线程安全加固、代码规范化** 四个方面，共涉及 12 个文件。

### 一、Bug 修复

#### 1. 全屏模式下倍速 ComboBox / 字幕弹窗无法弹出（`app/mainwindow.cpp`、`app/mainwindow.h`、`app/subtitlepopup.cpp`）

- **问题**：全屏时 `controlBarContainer` 被 `setParent` 到 `videoWidget`（QOpenGLWidget），导致其子控件的 `Qt::Popup` 窗口（QComboBox 下拉列表、SubtitlePopup）被 OpenGL 渲染表面遮挡，无法弹出。
- **修复**：全屏时不再将 `controlBarContainer` 设为 QOpenGLWidget 的子控件，改为从布局中移除后保持在 `centralwidget` 下，通过绝对定位 + `raise()` 悬浮在视频底部。新增 `repositionControlBarFullScreen()` 方法，在 `resizeEvent` 中同步更新位置。
- **修复**：为 `SubtitlePopup` 的窗口标志增加 `Qt::WindowStaysOnTopHint`，确保全屏时弹窗不被遮挡。

#### 2. 空字符串判断方式修正（`app/mainwindow.cpp`）

- 将 `filePath == nullptr` 和 `rtsp_url == nullptr` 改为 `filePath.isEmpty()` 和 `rtsp_url.isEmpty()`，避免 QString 与空指针比较的未定义行为。
- 将 `player_->setDecodeType(NULL)` 改为 `player_->setDecodeType(QString())`，使用正确的空 QString 代替 NULL。

### 二、资源泄漏修复

#### 1. PlayerCore 初始化失败时资源未释放（`core/playercore.cpp`）

- 在解复用器打开失败、视频解码器初始化失败、音频解码器初始化失败时，补充调用 `releaseResources()` 释放已分配的资源，防止内存泄漏。

#### 2. PictureCreator 预览图生成中 AVFrame 泄漏（`utils/picturecreator.cpp`）

- 在 `getPreViewImage()` 的多个提前返回路径中，补充 `av_frame_free(&frame)` 调用，修复 AVFrame 未释放的内存泄漏。
- 修复 AVPacket 的 `av_packet_unref` 调用位置，确保 break 前也正确释放 packet。

#### 3. 右键菜单 QAction 泄漏（`app/mainwindow.cpp`）

- 将右键菜单的 `QAction` 父对象从 `this` 改为 `popMenu`，并在添加 action 前先调用 `popMenu->clear()`，确保旧 action 随 clear 自动销毁，避免每次右键都泄漏。

#### 4. 析构函数资源释放顺序优化（`app/mainwindow.cpp`）

- 移除对 `player_` 和 `preview_` 的手动 delete（它们的 parent 设为 QObject 树节点，随父对象自动析构）。
- 调整 `delete ui` 到最后执行，确保 UI 子控件在其他清理逻辑之后再销毁。

#### 5. 移除调试用 PCM dump 文件（`render/audiooutput.cpp`、`render/audiooutput.h`）

- 移除 `dump_pcm_` 成员及相关的 `fopen`/`fwrite`/`fclose` 调用，清理遗留的调试代码，避免每次播放都在工作目录生成 `audio_dump.pcm` 文件。

### 三、线程安全加固

#### 1. seek 操作状态变更加锁（`core/playercore.cpp`）

- 在 `seek()` 中对 `is_seek_`、`state_` 的修改及 `cond_.wakeAll()` 加 `QMutexLocker` 保护，防止与解复用/解码线程的竞态条件（此前是 seek 后卡顿问题的根因之一）。

#### 2. 音频时钟更新加锁（`render/audiooutput.cpp`）

- 恢复 `audio_clock_us_` 赋值处的 `QMutexLocker` 保护，确保音频回调线程与主线程读取时钟时的线程安全。

#### 3. 队列 isEmpty() 加锁（`queue/avqueue.h`）

- 为模板队列的 `isEmpty()` 方法添加 `std::lock_guard` 互斥锁保护，防止在其他线程正在 Push/Pop 时读到不一致的状态。

### 四、代码规范化

#### 1. NULL 替换为 nullptr（`queue/avframequeue.cpp`、`queue/avpacketqueue.cpp`）

- 将队列模块中的 `NULL` 统一替换为 C++11 标准的 `nullptr`。

#### 2. Log 单例改为 Meyers' Singleton（`utils/log.cpp`、`utils/log.h`）

- 移除手动管理的 `static Log* m_instance` 指针和显式加锁，改用函数内局部静态变量（`static Log instance`）实现线程安全的懒初始化单例。
- 返回类型从 `Log*` 改为 `Log&`，宏调用从 `->` 改为 `.`，语义更清晰且避免空指针风险。
- 移除调试日志输出 `qDebug() << "audio pts : "` 。
