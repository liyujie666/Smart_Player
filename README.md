# Smart_Player

> 基于 **Qt 6.9 + FFmpeg + whisper.cpp** 的本地视频播放器，集成 AI 字幕、AI 语义分段、AI 总结 / 文稿跟随 等特性。
>
> 当前构建系统：**CMake**（`Smart_Player.pro` 仅作历史参考保留）。

---

## 1. 项目概览

| 项 | 说明 |
|----|------|
| 语言 / 标准 | C++17 |
| UI 框架 | Qt 6.9（Core / Gui / Widgets / OpenGL / Multimedia / Network / Svg / Concurrent） |
| 多媒体栈 | FFmpeg（avformat / avcodec / swscale / swresample / avfilter）+ SDL2 |
| ASR | whisper.cpp（自带 ggml-base / ggml-cpu） |
| 架构 | MVVM（View / ViewModel / Model 三层，渐进式重构中） |
| 平台 | Windows 10+，MinGW / MSVC |

---

## 2. 目录结构

```
Smart_Player/
├── CMakeLists.txt              # ★ 主构建脚本（CMake）
├── Smart_Player.pro            # 历史 qmake 工程，仅作参考
├── main.cpp                    # 程序入口
│
├── src/                       # 源码目录（所有业务代码）
│   ├── app/                    # View 层：QWidget 派生类、UI 控件、弹窗、配置 UI
│   ├── core/                   # 播放核心
│   ├── viewmodel/              # MVVM - ViewModel 层
│   ├── summary/                # AI 总结 / 文稿
│   ├── subtitle/               # ASR 字幕
│   ├── queue/                  # AV 帧 / 包队列
│   ├── render/                 # 渲染
│   ├── resampler/              # 重采样
│   ├── demuxer/               # 解封装
│   ├── decoder/                # 解码
│   ├── filter/                 # 滤镜
│   ├── converter/              # 视频转换（截图 / 缩略图）
│   ├── pool/                  # AVFrame / AVPacket 对象池
│   └── utils/                  # 通用工具
│
├── dependencies/               # 第三方依赖（本地副本）
│   ├── include/                # 头文件（FFmpeg / SDL2 / whisper / nlohmann / half_float）
│   ├── lib/                    # 导入库 .lib / .a
│   ├── bin/                    # 运行时 DLL（链接时复制到 exe 同级）
│   ├── models/                 # whisper ggml 模型文件
│   └── plugins/                # Qt 插件（platforms / imageformats / iconengines）
│
├── cmake/                      # CMake Find 模块
│   └── FindWhisper.cmake       # 自定义 whisper 查找器（处理 MinGW 导入库）
│
├── resources/                  # 应用资源
│   ├── Resource.qrc            # Qt 资源文件（图标 / gif）
│   ├── app_icon.rc             # Windows 资源：EXE 图标
│   ├── logo.ico                # 应用图标
│   └── SmartPlayer-icon/       # 图标图片（按钮 / 状态 / 加载动画）
│
├── scripts/                    # 构建 / 维护脚本
│   ├── build.bat               # 一键 CMake 构建（MinGW / Qt 6.9.0）
│   └── clean.bat               # 清理 build-cmake
│
├── docs/                       # 设计文档 / 分析报告
│   ├── MVVM_REFACTOR_PLAN.md
│   ├── 文稿面板设计方案.md
│   └── 内存安全分析报告.md
│
├── build-cmake/                # ★ CMake 构建产物（.gitignore，不会入库）
│
├── .gitignore
└── .agents/                    # Agent skills（本地）
```

---

## 3. 构建方式

### 3.1 一键脚本（推荐）

```bat
:: 项目根目录下
scripts\build.bat
```

等价于：

```bat
cmake -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe ^
    -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/mingw1310_64/bin/mingw32-make.exe ^
    -DQt6_DIR=C:/Qt/6.9.0/mingw_64/lib/cmake/Qt6 ^
    .
cd build-cmake
cmake --build . --parallel
```

输出：`build-cmake\Smart_Player.exe`

清理：

```bat
scripts\clean.bat
```

### 3.2 IDE

- **Qt Creator**：直接打开根目录的 `CMakeLists.txt`，选择构建套件即可。
- **CLion / VSCode**：配置 CMake 工具链指向 `D:/Qt/Tools/mingw1310_64/bin/g++.exe`，构建目录设为 `build-cmake/`。

### 3.3 qmake（不推荐，仅历史参考）

`Smart_Player.pro` 仍保留可用，但不再主动维护：

```bat
qmake Smart_Player.pro
mingw32-make
```

---

## 4. 第三方依赖说明

| 库 | 路径 | 用途 |
|----|------|------|
| FFmpeg | `dependencies/{include,lib,bin}/` | 解封装 / 解码 / 滤镜 / 重采样 |
| SDL2 | 同上 | 音频回调 |
| whisper.cpp + ggml | 同上 | ASR（ggml-base / ggml-cpu） |
| Qt 6.9.0 | `D:/Qt/6.9.0/mingw_64/` | 框架 |
| MinGW 13.1.0 | `D:/Qt/Tools/mingw1310_64/` | 工具链 |

CMake 会自动：
1. `add_custom_command(POST_BUILD)` 把 `dependencies/bin/*.dll` 复制到 exe 同级目录；
2. 把 `dependencies/plugins/*.dll` 复制到 exe 同级的 `platforms/`、`imageformats/`、`iconengines/`；
3. 从 `D:/Qt/6.9.0/mingw_64/plugins/tls/` 复制 `tls/` 插件（HTTPS 支持）。
4. `main.cpp` 启动时调用 `QCoreApplication::addLibraryPath()` 注册插件路径，并设置 `QT_TLS_BACKEND=schannel`（无需 OpenSSL DLL）。

---

## 5. 本次变更分析（`git diff`）

> **当前工作树相对 `origin/refactor/mvvm-project-arch-refactor` HEAD 的所有未提交改动。**
> 主要包含两块：**(A) qmake → CMake 迁移与第三方清理**，**(B) 工程目录整理**。

### 5.1 概览（`git diff --stat`）

```
 204 files changed, 2020 insertions(+), 44402 deletions(-)
```

净删除 4.2 万行，绝大多数来自**翻译模块的完全移除**（见 5.2）。

### 5.2 模块级改动

#### 5.2.1 翻译模块（translator/）—— 完全移除

| 删除项 | 文件数 | 行数 |
|--------|------|------|
| `translator/*.cpp` / `*.h` | 6 | ≈ 705 行 |
| `dependencies/include/ctranslate2/**` | 95 | ≈ 9.4k 行 |
| `dependencies/include/sentencepiece/**` | 2 | ≈ 967 行 |
| `video_summary.md` | 1 | 87 行 |

**动机**：CTranslate2 + SentencePiece 的依赖体积大、CUDA 编译选项硬编码（见原 `内存安全分析报告 3.13`），且 `translator/` 模块从未真正启用（在 `Smart_Player.pro` 里一直被 `HEADERS -=` / `SOURCES -=` 显式排除编译）。本次彻底清理由 cmake 化的同时一并完成。

**`.pro` 副作用**：

```diff
-LIBS += ... -lctranslate2 -lsentencepiece -lsentencepiece_train
+LIBS += -L$$PWD/dependencies/lib -lavcodec -lavdevice -lavfilter -lavformat ^
+        -lavutil -lpostproc -lswresample -lswscale -lSDL2 -lwhisper ^
+        -lggml -lggml-base -lggml-cpu
-# 显式排除未启用的 translator/ 模块
-HEADERS -= $$files($$PWD/translator/*.h,   false)
-SOURCES -= $$files($$PWD/translator/*.cpp, false)
```

#### 5.2.2 qmake → CMake 迁移

新增（**A**）：

| 文件 | 说明 |
|------|------|
| `CMakeLists.txt` | 主构建脚本（278 行） |
| `cmake/FindWhisper.cmake` | 自定义 Find 模块，处理 MinGW 导入库名（`whisper.lib` 而非 `-lwhisper`） |
| `build.bat` / `clean.bat` | 一键脚本（现位于 `scripts/`） |
| `build-cmake/` | cmake 构建产物目录（已加入 `.gitignore`） |

CMake 关键设计：

1. **GLOB 自动收集源码**：新增 `.cpp/.h/.ui` 不用改 `CMakeLists.txt`，与原 `.pro` 行为一致。
2. **链接顺序硬编码绝对路径**：避免 MinGW 下 `-lwhisper` 解析不到 `whisper.lib`（不带 `lib` 前缀）。
3. **INTERFACE target 包装 whisper**：`add_library(whisper_import INTERFACE)` 把 whisper + ggml 当一组链接。
4. **POST_BUILD 自动复制**：FFmpeg / Qt / TLS 插件 DLL 自动落到 exe 同级。
5. **TLS 插件路径三段探测**：CMake 缓存 → `Qt6_DIR` 反推 → 硬编码 `D:/Qt/6.9.0/mingw_64/plugins/tls` 兜底。

删除（**D**）：

| 文件 | 原因 |
|------|------|
| `Makefile` / `Makefile.Debug` / `Makefile.Release` | qmake 产物 |
| `.qmake.stash` | qmake 缓存 |
| `ui_mainwindow.h` 等 5 个 `ui_*.h` | qmake 的 uic 产物，cmake 用 `CMAKE_AUTOUIC` 重新生成 |
| `Smart_Player.pro.user` ×2 | QtCreator 用户配置（不应入库） |
| `CMakeLists.txt.user` | QtCreator 用户配置（已忽略） |
| `build/` / `debug/` / `release/` | qmake 构建树 |
| `full_review_report.json` / `skills-lock.json` / `build_log.txt` | 临时文件 |

#### 5.2.3 工程目录整理（本次新增）

| 原位置 | 新位置 | 原因 |
|--------|--------|------|
| `MVVM_REFACTOR_PLAN.md` / `内存安全分析报告.md` / `文稿面板设计方案.md` | `docs/` | 设计文档归类 |
| `build.bat` / `clean.bat` | `scripts/` | 构建脚本归类 |
| `Resource.qrc` / `app_icon.rc` / `logo.ico` | `resources/` | 资源文件归类 |
| `SmartPlayer-icon/` | `resources/SmartPlayer-icon/` | 资源子目录归类 |

`CMakeLists.txt` 与 `Smart_Player.pro` 同步更新为新路径：

```diff
 add_executable(${PROJECT_NAME} WIN32
     main.cpp
     ${ALL_SOURCES}
     ${ALL_HEADERS}
     ${APP_FORMS}
-    Resource.qrc
-    app_icon.rc
+    ${RESOURCES_DIR}/Resource.qrc
+    ${RESOURCES_DIR}/app_icon.rc
 )

 RESOURCES  += \
-    Resource.qrc
-RC_FILE = app_icon.rc
+    resources/Resource.qrc
+RC_FILE = resources/app_icon.rc
```

`scripts/build.bat` / `scripts/clean.bat` 中 `BUILD_DIR` 改为 `%SCRIPT_DIR%..\build-cmake`（脚本在子目录里，要回到项目根）。

### 5.3 代码级改动

| 文件 | 改动 | 说明 |
|------|------|------|
| `main.cpp` | +12 行 | 增加 `QCoreApplication::addLibraryPath` 显式注册 `plugins/` 和 `tls/`，并 `qputenv("QT_TLS_BACKEND", "schannel")` |
| `app/mainwindow.h` | +3 行 | 新增 `updateControlBarGeometry()` 私有方法 |
| `app/mainwindow.cpp` | +43 行 | 全屏态下控制栏宽度的同步逻辑（修复打开/关闭文件列表后控制栏错位） |
| `app/videoslider.cpp` | +14 行 | `mouseReleaseEvent` 末尾补 emit `sliderReleased()`，让"点击 seek"和"拖动 seek"行为一致 |
| `core/playercore.cpp` | -1 行 | 注释掉视频 pts 的 `qDebug` 噪声 |
| `render/audiooutput.cpp` | -2 行 | 注释掉音频 pts 的 `qDebug` 噪声 |
| `summary/summarypanel.cpp` | +66 行 | (1) 增加暗色滚动条样式；(2) `onRerunClicked` 真正跳过缓存重跑（之前只是 `onStartClicked`，无法换模型） |
| `summary/transcriptpanel.cpp` | +26 行 | 文稿面板同样的暗色滚动条样式 |

> 这些代码级改动与目录整理无关，但属于本次未提交的工作树状态；本次提交只包含**目录整理 + cmake 迁移**，代码改动应另起一次提交。

### 5.4 `.gitignore` 重写

| 旧规则 | 新增 |
|--------|------|
| 只忽略 qmake 产物（`build/` `debug/` `release/` `*.pro.user` 等） | 新增 cmake 产物：`build-cmake/`、`CMakeLists.txt.user`、`.qtc_clangd/`、`.cmake/`、`.qt/`、`CMakeFiles/`、`CMakeCache.txt` 等 |
| 缺少 cmake 临时目录 | 新增 `Testing/`、`cmake_install.cmake`、`qtcsettings.cmake`、`Smart_Player_autogen/`、`Smart_Player_autogen_timestamp_deps/` |
| 散乱的临时文件 | 显式列出 `audio_dump.pcm` / `crash.dmp` / `full_review_report.json` / `skills-lock.json` |

### 5.5 兼容性 / 风险点

| 风险 | 缓解 |
|------|------|
| `Smart_Player.pro` 中 `resources/Resource.qrc` / `resources/app_icon.rc` 的路径变更 | qmake 用相对路径，已同步更新；如有人仍用 qmake 构建，需要先 `qmake` 重新生成 Makefile |
| `scripts/build.bat` 中 `BUILD_DIR` 路径变更 | 已改用 `%SCRIPT_DIR%..\build-cmake` |
| `mainwindow.cpp` 中 `updateControlBarGeometry()` 是新方法 | 配套 `app/mainwindow.h` 已声明；非 ViewModel 重构，纯 UI 修复 |
| `cmake/FindWhisper.cmake` 强依赖 `dependencies/lib/whisper.lib` 路径 | 已在脚本中显式 `PATHS ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lib`，同时支持 `whisper_ROOT` 覆盖 |

### 5.6 验证

整理后实际跑了一遍 cmake + build：

```
$ cmake -G "MinGW Makefiles" -DQt6_DIR=... -S . -B build-cmake
-- Configuring done (0.3s)
-- Generating done (0.3s)

$ cmake --build build-cmake
[  4%] Automatic RCC for resources/Resource.qrc
[  6%] Building RC object CMakeFiles/.../resources/app_icon.rc.obj
[100%] Built target Smart_Player
```

新路径全部解析正确，链接成功。

---

## 6. 已知遗留 / TODO

| 项 | 状态 |
|----|------|
| `viewmodel/README.md` 中阶段 3-5（SummaryViewModel / SettingsViewModel / MainWindow < 400 行） | 未启动 |
| `内存安全分析报告.md` 中 High 18 个问题 | 待修复 |
| `Smart_Player.pro` 是否彻底下线 | 待决定（当前保留以备回滚） |
| `build/` `debug/` `release/` 等旧目录已被删除但 git 仍跟踪为 D（deleted） | 提交本次变更后即从 git 历史中清除 |

---

## 7. 变更日志

| 日期 | 提交 | 说明 |
|------|------|------|
| 2026-06-27 | （本次） | qmake → CMake 迁移完成；移除 translator/ 模块；目录整理 |
