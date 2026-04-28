#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "playercore.h"
#include "previewplayer.h"
#include "render/openglrenderer.h"
#include "videoslider.h"
#include "shotcutdialog.h"
#include "videoinfodialog.h"
#include "settingdialog.h"
#include "controlbarmanager.h"
#include "utils/picturecreator.h"
#include "videoitemwidget.h"
#include <QMainWindow>
#include <qlistwidget.h>
#include <QVector>
#include <QTimer>
#include <QMenu>
#include <QLabel>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void saveFile();
    void loadFile();
    void messageInfo(QString info,int interval);
    void openFileList();
    void closeFileList();
    void openVideoFromCommand(const QString &filePath);
    void addToFileList(QString filePath);
    void play(QString filePath);


private slots:
    void onPlayerStateChanged();    //播放器状态改变
    void onPlayerTimeChanged();      //进度条更新
    void onPlayerInitFinished();     //播放器初始化完成后，设置进度条和总时间
    void onPlayerPlayFailed(const QString& info);       //播放出错
    void onPlayerOpenResult(bool result);
    void onVideoSizeModeChanged(int mode);
    /*
        视频进度条
    */
    void onSliderClicked(VideoSlider *slider);          //seek
    void onSliderMouseFoucs(int seektime, int x);       //预览
    void onMouseLeaveSlider();                          //鼠标移动，预览消失
    void on_progressSlider_valueChanged(int value);
    void on_volumeSlider_valueChanged(int value);   //音量控制
    void on_openFileBtn_clicked();      //打开文件按钮
    void on_startBtn_clicked();         //播放按钮
    void on_stopBtn_clicked();          //停止播放按钮
    void on_preVideoBtn_clicked();      //切换上一个视频
    void on_nextVideoBtn_clicked();     //切换下一个视频
    void on_back3sBtn_clicked();        //后退3s
    void on_forward3sBtn_clicked();     //前进3s
    void on_volumeBtn_clicked();        //音量按钮
    void on_openListBtn_clicked(); //打开文件列表按钮
    void on_fullScreenBtn_clicked();    //全屏按钮
    void on_addFileBtn_clicked();       //添加文件按钮
    void on_addDirBtn_clicked();        //添加文件夹按钮
    void on_clearListBtn_clicked();     //清空文件夹按钮
    void on_mutipleSPeed_currentIndexChanged(int index);    //切换倍速
    void onLongPressTimeout();          //长按视频2倍速播放

    void on_fileList_itemDoubleClicked(QListWidgetItem *item);      //文件列表双击播放
    void on_rtspButton_clicked();                                   //设置网络流地址
    void on_screenShotBtn_clicked();                                //截屏
    void on_settingBtn_clicked();                                   //设置按钮，点击打开设置菜单
    void on_setHardWare(bool on);                                   //设置硬件加速
    void on_update_file_path(QString path);                         //更新文件保存路径
    void on_change_userDecoder(QString decoder);                    //切换用户指定解码器
    void on_show_message_info(QString info,int interval);           //显示提示消息
    void on_fileList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    void on_frameYuv420pDecoded(const QByteArray &data, int width, int height);
    void on_frameNv12Decoded(const QByteArray &data, int width, int height);
    void on_frameRGBADecoded(const QByteArray& rgbData,int width,int height);
    void on_previewFrameDecoded(const QByteArray& data, int w, int h, AVPixelFormat fmt);
    void on_progressSlider_sliderPressed();
    void on_progressSlider_sliderReleased();
    void on_screenshotStatus(const QString& path,bool isOk);

    void hideControlBarAndCursor();
    void showControlBarAndCursor();

    //void on_progressSlider_sliderMoved(int position);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    void initPreviewWindow();
    void initControlbarPresent();
    void initComponent();
    QString getTimeText(int64_t value);
    void mousePressEvent(QMouseEvent *event) override;//获取视频信息
    void mouseDoubleClickEvent(QMouseEvent *event) override;//鼠标双击全屏
    void keyPressEvent(QKeyEvent * event) override;//快捷键
    //void mouseMoveEvent(QMouseEvent *event) override;
    void keyReleaseEvent(QKeyEvent* event) override;//键盘松开
    void contextMenuEvent(QContextMenuEvent *pEvent) override;
    void restoreControlBarParent();
    void enterFullScreenMode();
    void exitFullScreenMode();
    void centerLoadingLabel();



private:
    Ui::MainWindow *ui;
    settingDialog *settingdialog;
    PlayerCore *player_;
    PreviewPlayer* preview_player_;
    OpenGLRenderer *preview_;
    QWidget *previewContainer_ = nullptr;
    QLabel *previewTimeLabel_ = nullptr;

    QMenu *popMenu;
    PictureCreator* picture_;


    QVector<QString> fileList;//文件列表
    int listIndex;//当前播放文件在播放列表中位置
    QTimer timer; // 定时器
    bool isLongPress = false; // 是否长按
    bool is_seeking = false;


    QTimer *hideCursorTimer;       // 3秒定时隐藏
    bool isFullScreenMode;         // 全屏标记
    QString originalControlBarStyle;// 保存控制栏原始样式
    bool m_isMouseOverControlBar = false;

    // 加载动画
    QLabel* loadingLabel_ = nullptr;


};
#endif // MAINWINDOW_H
