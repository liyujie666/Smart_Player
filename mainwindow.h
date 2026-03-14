#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "videoplayer.h"
#include "videoslider.h"
#include "videowidget.h"
#include "shotcutdialog.h"
#include "videoinfodialog.h"
#include "settingdialog.h"
#include "controlbarmanager.h"
#include "picturecreator.h"
#include "videoitemwidget.h"
#include <QMainWindow>
#include <qlistwidget.h>
#include <QVector>
#include <QTimer>
#include <QMenu>
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
    void onPlayerStateChanged(VideoPlayer *player);    //播放器状态改变
    void onPlayerTimeChanged(VideoPlayer *player);      //进度条更新
    void onPlayerInitFinished(VideoPlayer *player);     //播放器初始化完成后，设置进度条和总时间
    void onPlayerPlayFailed(VideoPlayer *player);       //播放出错
    /*
        视频进度条
    */
    void onSliderClicked(VideoSlider *slider);          //seek
    void onSliderMouseFoucs(int seektime, int x);       //预览
    void onMouseLeaveSlider();                          //鼠标移动，预览消失
    void on_progressSlider_valueChanged(int value);

    /*
        音量进度条
    */
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

signals:

private:
    Ui::MainWindow *ui;
    settingDialog *settingdialog;
    VideoPlayer *player_, *preview_player_;
    VideoWidget *preview_;
    QString getTimeText(int value);
    QMenu *popMenu;
    PictureCreator* picture_;


    QVector<QString> fileList;//文件列表
    void mousePressEvent(QMouseEvent *event) override;//获取视频信息
    void mouseDoubleClickEvent(QMouseEvent *event) override;//鼠标双击全屏
    void keyPressEvent(QKeyEvent * event) override;//快捷键
    void keyReleaseEvent(QKeyEvent* event) override;//键盘松开
    void contextMenuEvent(QContextMenuEvent *pEvent) override;
    void restoreControlBarParent();
    int listIndex;//当前播放文件在播放列表中位置
    QTimer timer; // 定时器
    bool isLongPress = false; // 是否长按


};
#endif // MAINWINDOW_H
