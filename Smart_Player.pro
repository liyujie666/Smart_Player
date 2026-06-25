QT += core gui opengl openglwidgets concurrent network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
# CONFIG += debug


# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# ===== 模块清单 =====
#   app/         主窗口、UI 控件、弹窗、配置
#   core/        播放核心 (playercore + syncclock)
#   viewmodel/   MVVM 中的 ViewModel 层（连接 View 与 core/summary 等 Model）
#   summary/     AI 总结 / 分段 / 网络 / 面板 / 文稿
#   subtitle/    ASR 字幕生成
#   translator/  翻译（当前禁用编译，依赖未配置）
#   queue/       AV 帧/包队列
#   render/      音视频渲染
#   resampler/   重采样
#   demuxer/     解封装
#   decoder/     解码
#   filter/      滤镜
#   converter/   视频转换
#   pool/        帧/包池
#   utils/       工具
#   screenshot/  截图

# ===== 自动收集源码（新增/删除文件不用再改这里）=====
# 收集所有源码目录
SRC_DIRS = $$PWD \
           $$PWD/app \
           $$PWD/core \
           $$PWD/viewmodel \
           $$PWD/summary \
           $$PWD/subtitle \
           $$PWD/queue \
           $$PWD/render \
           $$PWD/resampler \
           $$PWD/demuxer \
           $$PWD/decoder \
           $$PWD/filter \
           $$PWD/converter \
           $$PWD/pool \
           $$PWD/utils \
           $$PWD/screenshot

for(dir, SRC_DIRS) {
    SOURCES += $$files($$dir/*.cpp, false)
    HEADERS += $$files($$dir/*.h,   false)
}
FORMS    += $$files($$PWD/app/*.ui,       false) \
            $$files($$PWD/summary/*.ui,   false)

# 显式排除未启用的 translator/ 模块
HEADERS -= $$files($$PWD/translator/*.h,   false)
SOURCES -= $$files($$PWD/translator/*.cpp, false)

RESOURCES  += \
    Resource.qrc

RC_FILE = app_icon.rc

# 让所有 .cpp/.h 可以用短名 #include "foo.h" 找到任何模块的头
INCLUDEPATH += $$SRC_DIRS \
               $$PWD/dependencies/include

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

LIBS        += -L$$PWD/dependencies/lib -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lpostproc -lswresample -lswscale -lSDL2 -lwhisper -lggml -lggml-base -lggml-cpu -lctranslate2 -lsentencepiece -lsentencepiece_train
LIBS += -ldbghelp
DEFINES += SDL_MAIN_HANDLED
DISTFILES += \
    app_icon.rc
