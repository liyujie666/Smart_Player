# viewmodel/ —— MVVM 中的 ViewModel 层

> 项目 MVVM 渐进式重构的产物。本目录内的类**不允许**`#include` 任何 `app/` 下的 UI 头文件（含 `ui_xxx.h`、`QWidget` 派生类的私有头）。
> ViewModel 只依赖 `core/ summary/ subtitle/ translator/` 等 Model 层模块。

## 现有 ViewModel

| 类 | 封装的 Model | 主要职责 |
|---|---|---|
| `PlayerViewModel` | `core/PlayerCore` | 播放控制、状态/位置/音量等可观察属性、视频帧透传 |
| `PlaylistViewModel` | `app/ConfigManager`（持久化） | tracks/currentIndex/playMode；增删/上一首下一首/Shuffle/SingleRepeat 等 |

## 设计要点

1. **可观察状态** → `Q_PROPERTY` + `NOTIFY signal`，View 只通过 `connect(vm, &Vm::xxxChanged, ...)` 更新自己。
2. **用户动作** → `public slot`（Command），View 调用 vm 的槽而不是直接 `model->xxx()`。
3. **业务事件**（非属性的一次性通知，如"打开成功"）→ 普通 `signal`。
4. **视频帧/音频流等大流量数据** → 直接透传，不解释。

## View 写法约定

```cpp
// 旧（与 Model 耦合）：
connect(player_, &PlayerCore::stateChanged, this, &MainWindow::onPlayerStateChanged);
if (player_->state() == PlayerCore::Running) { ... }

// 新（仅与 ViewModel 交互）：
connect(player_, &PlayerViewModel::stateChanged, this, &MainWindow::onPlayerStateChanged);
if (player_->state() == PlayerViewModel::Running) { ... }
```

`PlayerViewModel` 信号名与原 `PlayerCore` 保持一致，便于已有 connect 平滑迁移。

## 后续路线

- ✅ 阶段 1：`PlayerViewModel` 抽离完成
- ✅ 阶段 2：`PlaylistViewModel` 抽离完成（`MainWindow::fileList / listIndex / playMode_ / shuffledList_ / fileDurationList` 已全部移除）
- 阶段 3：`SummaryViewModel` —— 包装 `VideoSummaryManager`，`SummaryPanel / TranscriptPanel` 改为纯 View。
- 阶段 4：`SettingsViewModel` —— 合并 `ConfigManager` + `settingDialog` 的运行时配置。
- 阶段 5：`MainWindow` 瘦身到 < 400 行。
