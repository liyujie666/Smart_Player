#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "viewmodel/playerviewmodel.h"
#include "viewmodel/playlistviewmodel.h"
#include "previewplayer.h"
#include "render/openglrenderer.h"
#include "videoslider.h"
#include "shotcutdialog.h"
#include "videoinfodialog.h"
#include "settingdialog.h"
#include "controlbarmanager.h"
#include "utils/picturecreator.h"
#include "videoitemwidget.h"
#include "subtitlepopup.h"
#include "configmanager.h"
#include "viewmodel/summaryviewmodel.h"
#include "viewmodel/transcriptviewmodel.h"
#include "summarypanel.h"
#include "transcriptpanel.h"
#include <QMainWindow>
#include <qlistwidget.h>
#include <QDockWidget>
#include <QVector>
#include <QTimer>
#include <QMenu>
#include <QLabel>
class SlidingTabWidget;
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
    void on_playFinished();

    void on_frameYuv420pDecoded(const QByteArray &data, int width, int height);
    void on_frameNv12Decoded(const QByteArray &data, int width, int height);
    void on_frameRGBADecoded(const QByteArray& rgbData,int width,int height);
    void on_previewFrameDecoded(const QByteArray& data, int w, int h, AVPixelFormat fmt);
    void on_subtitleReady(const QString& text);
    void on_progressSlider_sliderPressed();
    void on_progressSlider_sliderReleased();
    void on_screenshotStatus(const QString& path,bool isOk);

    void hideControlBarAndCursor();
    void showControlBarAndCursor();

    //void on_progressSlider_sliderMoved(int position);

    void on_subtitleBtn_clicked();

    void on_switchPlayModeBtn_clicked();

    void on_modelPathChanged(const QString& path);
    void saveAllSettings();
    void loadVideoList();
    void scheduleSave();

private slots:
    void onSaveDebounceTimeout();
    void onThumbBatchTimeout();
    void onThumbLoaded(const QString& path, const QImage& thumb);

    void on_aiSummaryBtn_clicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    void initPreviewWindow();
    void initControlbarPresent();
    void initComponent();
    void applyPersistentSettings();
    QString getTimeText(int64_t value);
    QListWidgetItem* findItemByPath(const QString& path);
    void seekRelative(int seconds);  // 以当前位置为基准相对偏移 seek
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
    void repositionControlBarFullScreen();

    void setupRightPanel();



private:
    Ui::MainWindow *ui;
    settingDialog *settingdialog;
    PlayerViewModel *player_;   // MVVM 阶段 1：原 PlayerCore* 升级为 PlayerViewModel*
    PreviewPlayer* preview_player_;
    OpenGLRenderer *preview_;
    QWidget *previewContainer_ = nullptr;
    QLabel *previewTimeLabel_ = nullptr;
    QMenu *popMenu = nullptr;
    PictureCreator* picture_ = nullptr;
    SubtitlePopup *subtitlePopup_ = nullptr;


    // 播放列表迁移到 PlaylistViewModel；fileList/fileDurationList/listIndex/shuffledList_/playMode_
    // 等字段已被废弃。新代码请通过 playlist_->xxx() 访问。
    PlaylistViewModel* playlist_ = nullptr;
    std::atomic<bool> playFinished_busy_{false};
    QTimer timer; // 定时器
    bool isLongPress = false; // 是否长按
    bool is_seeking = false;
    bool isMutedByUser_ = false;     // 用户期望的静音状态（切视频后保留）


    QTimer *hideCursorTimer;       // 3秒定时隐藏
    QTimer saveDebounceTimer_;      // 防抖保存定时器
    QTimer thumbLazyTimer_;         // 延迟加载缩略图批次
    int thumbLoadStartIdx_ = 0;     // 缩略图延迟加载起始索引
    bool isFullScreenMode;         // 全屏标记
    QString originalControlBarStyle;// 保存控制栏原始样式
    bool m_isMouseOverControlBar = false;

    // 加载动画
    QLabel* loadingLabel_ = nullptr;

    SummaryViewModel*    m_summaryVm       = nullptr;   // MVVM 阶段 3a：替换原 m_summaryManager
    TranscriptViewModel* m_transcriptVm    = nullptr;   // MVVM 阶段 3b：拆解 TranscriptPanel
    SummaryPanel*        m_summaryPanel    = nullptr;
    TranscriptPanel*     m_transcriptPanel = nullptr;
    QDockWidget*         m_rightDock       = nullptr;   // 合并后的右侧 dock（内含 tab）
    SlidingTabWidget*    m_rightTab        = nullptr;   // AI 总结 / 视频文稿 两个 tab（带滑块动画）

};
#endif // MAINWINDOW_H
