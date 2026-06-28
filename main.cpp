#include "mainwindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>

#ifdef Q_OS_WIN
#  include <qpa/qplatformwindow_p.h>
#  include <QWindow>
#endif

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

#ifdef Q_OS_WIN
    // Windows + OpenGL 全屏修复：videoWidget 是 QOpenGLWidget，MainWindow
    // 进入 showFullScreen() 时整窗口变成 OpenGL surface，Windows DWM 会让
    // popup 类顶层窗口（QComboBox 下拉、Qt::Popup 弹窗、菜单/对话框）无法
    // 合成到主窗口之上 —— 全屏时点击倍速 combobox 不弹、点击 AI 字幕按钮
    // SubtitlePopup 不显示。
    //
    // 必须在窗口首次切换到全屏之前调用 setHasBorderInFullScreen(true)，让 Qt
    // Windows 平台插件给全屏窗口 HWND 加上 WS_BORDER，从而让 DWM 走完整的合成
    // 路径。QWidget::windowHandle() 在 show() 之后才存在，所以放在 show() 之后。
    // 标志本身保存在 QWindow 上，后续 showFullScreen() 会读取并应用。
    // 见 https://doc.qt.io/qt-6/windows-issues.html#fullscreen-opengl-based-windows
    if (auto inf = w.windowHandle()
                       ? w.windowHandle()->nativeInterface< QNativeInterface::Private::QWindowsWindow >()
                       : nullptr) {
        inf->setHasBorderInFullScreen(true);
    }
#endif

    return a.exec();
}
