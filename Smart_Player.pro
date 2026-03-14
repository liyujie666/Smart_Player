QT += core gui opengl concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
# CONFIG += debug


# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    condmutex.cpp \
    main.cpp \
    mainwindow.cpp \
    picturecreator.cpp \
    settingdialog.cpp \
    shotcutdialog.cpp \
    videoinfodialog.cpp \
    videoitemwidget.cpp \
    videoplayer.cpp \
    videoplayer_audio.cpp \
    videoplayer_video.cpp \
    videoslider.cpp \
    videowidget.cpp

HEADERS += \
    condmutex.h \
    mainwindow.h \
    picturecreator.h \
    settingdialog.h \
    shotcutdialog.h \
    videoinfodialog.h \
    videoitemwidget.h \
    videoplayer.h \
    videoslider.h \
    videowidget.h

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

INCLUDEPATH += $$PWD/include

LIBS        += -L$$PWD/lib -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lpostproc -lswresample -lswscale -lSDL2main -lSDL2

DISTFILES += \
    app_icon.rc
