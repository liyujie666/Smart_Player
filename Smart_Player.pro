QT += core gui opengl openglwidgets concurrent network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Windows + OpenGL 全屏修复需要用到 QNativeInterface::Private::QWindowsWindow，
# 这是 Qt 私有接口，仅在 Windows 下需要。
win32: QT += gui-private

CONFIG += c++17
# CONFIG += debug


# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# ===== 模块清单（src/ 下）=====
#   src/app/         主窗口、UI 控件、弹窗、配置
#   src/core/        播放核心 (playercore + syncclock)
#   src/viewmodel/   MVVM 中的 ViewModel 层（连接 View 与 core/summary 等 Model）
#   src/summary/     AI 总结 / 分段 / 网络 / 面板 / 文稿
#   src/subtitle/    ASR 字幕生成
#   src/queue/       AV 帧/包队列
#   src/render/      音视频渲染
#   src/resampler/   重采样
#   src/demuxer/     解封装
#   src/decoder/     解码
#   src/filter/      滤镜
#   src/converter/   视频转换
#   src/pool/        帧/包池
#   src/utils/       工具（含截图）

# ===== 自动收集源码（新增/删除文件不用再改这里）=====
# 收集所有源码目录
SRC_DIRS = $$PWD \
           $$PWD/src \
           $$PWD/src/app \
           $$PWD/src/core \
           $$PWD/src/viewmodel \
           $$PWD/src/summary \
           $$PWD/src/subtitle \
           $$PWD/src/queue \
           $$PWD/src/render \
           $$PWD/src/resampler \
           $$PWD/src/demuxer \
           $$PWD/src/decoder \
           $$PWD/src/filter \
           $$PWD/src/converter \
           $$PWD/src/pool \
           $$PWD/src/utils

for(dir, SRC_DIRS) {
    SOURCES += $$files($$dir/*.cpp, false)
    HEADERS += $$files($$dir/*.h,   false)
}
FORMS    += $$files($$PWD/src/app/*.ui,       false) \
            $$files($$PWD/src/summary/*.ui,   false)

RESOURCES  += \
    resources/Resource.qrc

RC_FILE = resources/app_icon.rc

# 让所有 .cpp/.h 可以用短名 #include "foo.h" 找到任何模块的头
INCLUDEPATH += $$SRC_DIRS \
               $$PWD/dependencies/include

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# ===== 第三方库依赖路径 =====
# 编译链接时：lib/*.lib
# 运行时加载：bin/*.dll → 需要加入 PATH 或复制到可执行文件同目录
LIBS        += -L$$PWD/dependencies/lib -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lpostproc -lswresample -lswscale -lSDL2 -lwhisper -lggml -lggml-base -lggml-cpu

# 运行时 DLL 搜索路径（exe 同目录的依赖）
win32: LIBS += -L$$PWD/dependencies/bin

LIBS += -ldbghelp
DEFINES += SDL_MAIN_HANDLED
DISTFILES += \
    resources/app_icon.rc
