QT += core gui opengl openglwidgets concurrent network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
# CONFIG += debug


# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    configmanager.cpp \
    videosummarymanager.cpp \
    videosummarysegmenter.cpp \
    videosummarynetworkclient.cpp \
    semanticsegmenter.cpp \
    summarysettingsdialog.cpp \
    subtitle/asrmanager.cpp \
    subtitle/asrofflinestrategy.cpp \
    subtitle/asrrealtimestrategy.cpp \
    subtitle/asrworker.cpp \
    #translator/subtitletranslator.cpp \
    #translator/ctranslate2backend.cpp \
    #translator/translatorintegration.cpp \
    main.cpp \
    mainwindow.cpp \
    subtitlepopup.cpp \
    flowlayout.cpp \
    summarypanel.cpp \
    utils/audioringbuffer.cpp \
    utils/picturecreator.cpp \
    utils/log.cpp \
    previewplayer.cpp \
    queue/avframequeue.cpp \
    queue/avpacketqueue.cpp \
    queue/avqueue.cpp \
    render/audiooutput.cpp \
    render/openglrenderer.cpp \
    resampler/resampler.cpp \
    settingdialog.cpp \
    shotcutdialog.cpp \
    videoinfodialog.cpp \
    videoitemwidget.cpp \
    videoslider.cpp \
    demuxer/demuxer.cpp \
    decoder/decoder.cpp \
    filter/audiofilter.cpp \
    converter/videoconverter.cpp \
    pool/framepool.cpp \
    pool/packetpool.cpp \
    playercore.cpp

HEADERS += \
    configmanager.h \
    videosummarymanager.h \
    videosummarysegmenter.h \
    videosummarynetworkclient.h \
    semanticsegmenter.h \
    summarysettingsdialog.h \
    subtitle/asrmanager.h \
    subtitle/asrofflinestrategy.h \
    subtitle/asrrealtimestrategy.h \
    subtitle/asrworker.h \
    #translator/subtitletranslator.h \
    #translator/ctranslate2backend.h \
    #translator/translatorintegration.h \
    mainwindow.h \
    flowlayout.h \
    summarypanel.h \
    queue/subtitlequeue.h \
    subtitle/iasrstrategy.h \
    subtitlepopup.h \
    utils/asrutils.h \
    utils/audioringbuffer.h \
    utils/picturecreator.h \
    utils/log.h \
    previewplayer.h \
    queue/avframequeue.h \
    queue/avpacketqueue.h \
    queue/avqueue.h \
    render/audiooutput.h \
    render/openglrenderer.h \
    resampler/resampler.h \
    settingdialog.h \
    shotcutdialog.h \
    syncclock.h \
    videoinfodialog.h \
    videoitemwidget.h \
    videoslider.h \
    demuxer/demuxer.h \
    decoder/decoder.h \
    filter/audiofilter.h \
    converter/videoconverter.h \
    pool/framepool.h \
    pool/globalpool.h \
    pool/packetpool.h \
    playercore.h

FORMS += \
    mainwindow.ui \
    settingdialog.ui \
    shotcutdialog.ui \
    videoinfodialog.ui \
    videoitemwidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    Resource.qrc

RC_FILE = app_icon.rc

INCLUDEPATH += $$PWD/dependencies/include

LIBS        += -L$$PWD/dependencies/lib -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lpostproc -lswresample -lswscale -lSDL2 -lwhisper -lggml -lggml-base -lggml-cpu -lctranslate2 -lsentencepiece -lsentencepiece_train
LIBS += -ldbghelp
DEFINES += SDL_MAIN_HANDLED
DISTFILES += \
    app_icon.rc
