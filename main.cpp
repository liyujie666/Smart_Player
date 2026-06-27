#include "mainwindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 设置 Qt 插件路径（相对于可执行文件）
    // Qt 6.9+ 默认不会扫描 plugins/ 子目录，需显式 addLibraryPath
    QCoreApplication::addLibraryPath(a.applicationDirPath() + "/plugins");
    QCoreApplication::addLibraryPath(a.applicationDirPath());

    // 优先使用 Windows 自带的 schannel 作为 TLS 后端（无需额外 OpenSSL DLL）
    // 如果失败会自动回退到 OpenSSL（需要 libcrypto-3-x64.dll / libssl-3-x64.dll）
    qputenv("QT_TLS_BACKEND", "schannel");

    MainWindow w;
    w.show();
    return a.exec();
}
