#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "slidingtabwidget.h"
#include <algorithm>
#include <random>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <thread>
#include <time.h>
#include <QDebug>
#include <QMessageBox>
#include <qlistwidget.h>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QMouseEvent>
#include <QPushButton>
#include <QMovie>
#include <QTimer>
#include <QtConcurrent>
#include <QDockWidget>
#include <QAction>
#include <QMenu>
#include <QMenuBar>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)

{

    ui->setupUi(this);
    this->setWindowTitle("Smart_Player");
    this->setWindowIcon(QIcon(":/SmartPlayer-icon/logo.png"));
    ui->videoWidget->installEventFilter(this);
    ui->controlBarContainer->installEventFilter(this);
    ui->videoWidget->setMouseTracking(true);
    ui->controlBarContainer->setMouseTracking(true);
    // 全屏模式下：fileList / playerPage 的 Resize/Show/Hide 也需要 eventFilter 监听，
    // 才能在打开/关闭文件列表时同步重定位 controlBarContainer。
    ui->fileList->installEventFilter(this);
    ui->playerPage->installEventFilter(this);
    setContentsMargins(0, 0, 0, 0);
    settingdialog = new settingDialog(this);
    popMenu = new QMenu(this);
    picture_ = new PictureCreator();

    initPreviewWindow();
    initControlbarPresent();
    initComponent();

    player_ = new PlayerViewModel(this);   // 由 VM 内部持有 PlayerCore 生命周期
    preview_player_ = new PreviewPlayer();

    // === PlaylistViewModel：列表数据全权代理 ===
    // View 只订阅 VM 的信号去维护 QListWidget 的可视项；不再持有 fileList/listIndex/playMode_ 等字段。
    playlist_ = new PlaylistViewModel(this);

    // 单个 track 插入：在 QListWidget 末尾追加一个项
    connect(playlist_, &PlaylistViewModel::trackAdded, this,
            [this](int /*index*/, const PlaylistViewModel::Track& t) {
                QImage thumb = t.thumbnailPath.isEmpty()
                                   ? picture_->getPreViewImage(t.path, 110, 65)
                                   : QImage(t.thumbnailPath);
                if (thumb.isNull()) {
                    thumb = QImage(110, 65, QImage::Format_ARGB32);
                    thumb.fill(Qt::darkGray);
                }
                int dur = t.durationSec > 0 ? t.durationSec : picture_->duration(t.path);
                VideoItemWidget* itemWidget = new VideoItemWidget(thumb, QFileInfo(t.path).fileName(), getTimeText(dur));
                QListWidgetItem* item = new QListWidgetItem(ui->fileList);
                item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
                item->setBackground(Qt::transparent);
                item->setSizeHint(itemWidget->sizeHint());
                item->setData(Qt::UserRole, t.path);
                ui->fileList->addItem(item);
                ui->fileList->setItemWidget(item, itemWidget);
                ui->fileList->update();
            });

    // 单条删除
    connect(playlist_, &PlaylistViewModel::trackRemoved, this, [this](int index) {
        if (index < 0 || index >= ui->fileList->count()) return;
        delete ui->fileList->takeItem(index);
    });

    // 清空
    connect(playlist_, &PlaylistViewModel::cleared, this, [this]() {
        ui->fileList->clear();
    });

    // tracksReset：先清空 View，再由 trackAdded 信号逐条插入。loadFromConfig 内部不会重复 emit
    // trackAdded（避免列表抖动）——所以这里在 tracksReset 后手动从 VM 拉一遍重建。
    connect(playlist_, &PlaylistViewModel::tracksReset, this, [this]() {
        ui->fileList->clear();
        for (int i = 0; i < playlist_->count(); ++i) {
            const PlaylistViewModel::Track* t = playlist_->at(i);
            if (!t) continue;
            QImage thumb;
            if (!t->thumbnailPath.isEmpty() && QFileInfo::exists(t->thumbnailPath)) {
                thumb.load(t->thumbnailPath);
            }
            if (thumb.isNull()) {
                thumb = QImage(110, 65, QImage::Format_ARGB32);
                thumb.fill(Qt::darkGray);
            }
            int dur = t->durationSec > 0 ? t->durationSec : picture_->duration(t->path);
            VideoItemWidget* itemWidget = new VideoItemWidget(thumb, QFileInfo(t->path).fileName(), getTimeText(dur));
            QListWidgetItem* item = new QListWidgetItem(ui->fileList);
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            item->setBackground(Qt::transparent);
            item->setSizeHint(itemWidget->sizeHint());
            item->setData(Qt::UserRole, t->path);
            ui->fileList->addItem(item);
            ui->fileList->setItemWidget(item, itemWidget);
        }
    });

    // 当前项变化：同步 QListWidget 高亮行
    connect(playlist_, &PlaylistViewModel::currentIndexChanged, this, [this](int idx) {
        if (idx >= 0 && idx < ui->fileList->count()) {
            ui->fileList->setCurrentRow(idx);
        }
    });

    // 播放模式变化：切换按钮图标 & ToolTip
    connect(playlist_, &PlaylistViewModel::playModeChanged, this, [this](PlayMode mode) {
        switch (mode) {
        case PlayMode::ListLoop:
            ui->switchPlayModeBtn->setIcon(QIcon(":/SmartPlayer-icon/list_circle.png"));
            ui->switchPlayModeBtn->setToolTip(QString::fromUtf8("列表循环"));
            break;
        case PlayMode::SingleRepeat:
            ui->switchPlayModeBtn->setIcon(QIcon(":/SmartPlayer-icon/single_circle.png"));
            ui->switchPlayModeBtn->setToolTip(QString::fromUtf8("单曲循环"));
            break;
        case PlayMode::Shuffle:
            ui->switchPlayModeBtn->setIcon(QIcon(":/SmartPlayer-icon/random_circle.png"));
            ui->switchPlayModeBtn->setToolTip(QString::fromUtf8("随机播放"));
            break;
        }
    });

    // 用户意图：VM 表示"请播放这一首" → 由 View 触发实际打开
    connect(playlist_, &PlaylistViewModel::currentTrackRequested, this, [this](const QString& path) {
        play(path);
    });

    //加载曾经播放过的文件
    ConfigManager::instance().load();
    loadVideoList();

    saveDebounceTimer_.setSingleShot(true);
    connect(&saveDebounceTimer_, &QTimer::timeout, this, &MainWindow::onSaveDebounceTimeout);
    thumbLazyTimer_.setSingleShot(true);
    thumbLazyTimer_.setInterval(50);
    connect(&thumbLazyTimer_, &QTimer::timeout, this, &MainWindow::onThumbBatchTimeout);

    setFocusPolicy(Qt::StrongFocus); //获取键盘监听
    ui->speedLabel->setVisible(false);
    ui->infoLabel->hide();

    setupRightPanel();

    connect(&timer,&QTimer::timeout,this,&MainWindow::onLongPressTimeout);   //右方向键长按倍速播放
    connect(player_, &PlayerViewModel::openResult,this,&MainWindow::onPlayerOpenResult);//视频渲染
    connect(player_, &PlayerViewModel::stateChanged,this,&MainWindow::onPlayerStateChanged); //播放器状态转换
    connect(player_,&PlayerViewModel::timeChanged,this,&MainWindow::onPlayerTimeChanged); //更新当前播放时间
    connect(player_,&PlayerViewModel::initFinished,this,&MainWindow::onPlayerInitFinished); //初始化播放器参数
    connect(player_,&PlayerViewModel::playFailed,this,&MainWindow::onPlayerPlayFailed); //播放出错
    connect(player_,&PlayerViewModel::screecshotStatus,this,&MainWindow::on_screenshotStatus);
    connect(player_, &PlayerViewModel::playFinished, this, &MainWindow::on_playFinished);
    connect(player_,&PlayerViewModel::frameYuv420pDecoded,this,&MainWindow::on_frameYuv420pDecoded);//视频渲染
    connect(player_,&PlayerViewModel::frameNv12Decoded,this,&MainWindow::on_frameNv12Decoded);//视频渲染
    connect(player_,&PlayerViewModel::frameRGBADecoded,this,&MainWindow::on_frameRGBADecoded);//视频渲染
    connect(player_, &PlayerViewModel::subtitleReady,this, &MainWindow::on_subtitleReady);
    connect(preview_player_,&PreviewPlayer::previewFrameReady,this,&MainWindow::on_previewFrameDecoded);

    connect(ui->progressSlider, &VideoSlider::clicked,this, &MainWindow::onSliderClicked);  //进度条点击
    connect(ui->progressSlider,&VideoSlider::preview,this,&MainWindow::onSliderMouseFoucs);    //进度条鼠标悬停（显示预览图片
    connect(ui->progressSlider,&VideoSlider::mouseleave,this,&MainWindow::onMouseLeaveSlider);    //进度条鼠标移动（关闭预览图片）
    connect(settingdialog, &settingDialog::startHardWareAccep, this, &MainWindow::on_setHardWare);          //设置硬件加速
    connect(settingdialog, &settingDialog::startSoftWareAccep, this, &MainWindow::on_setHardWare);          //设置软件加速
    connect(settingdialog,&settingDialog::updateSaveFilePath,this,&MainWindow::on_update_file_path);        //更新文件保存路径
    connect(settingdialog,&settingDialog::updateUserDecoder,this,&MainWindow::on_change_userDecoder);       //切换用户指定解码器
    connect(settingdialog, &settingDialog::updateVideoSizeMode,this, &MainWindow::onVideoSizeModeChanged);
    connect(settingdialog, &settingDialog::brightnessValueChanged, this, [this](int value){
        float b = value / 100.0f;
        ui->videoWidget->setBrightness(b);
    });

    connect(settingdialog, &settingDialog::contrastValueChanged, this, [this](int value){
        float c = value / 100.0f;
        ui->videoWidget->setContrast(c);
    });

    connect(settingdialog, &settingDialog::saturationValueChanged, this, [this](int value){
        float s = value / 100.0f;
        ui->videoWidget->setSaturation(s);
    });

    connect(settingdialog, &settingDialog::updateModelPath, this, &MainWindow::on_modelPathChanged);

    //音量设置
    ui->volumeSlider->setRange(0, 100);
    ui->volumeSlider->setValue(ui->volumeSlider->maximum() >> 1);

    ui->progressSlider->setEnabled(false);

    //设置mutipleSpeed当前索引，并将文本对齐方式设置为居中
    ui->mutipleSPeed->setCurrentIndex(2);
    static_cast<QStandardItemModel*>(ui->mutipleSPeed->model())->item(0)->setTextAlignment(Qt::AlignCenter);
    static_cast<QStandardItemModel*>(ui->mutipleSPeed->model())->item(1)->setTextAlignment(Qt::AlignCenter);
    static_cast<QStandardItemModel*>(ui->mutipleSPeed->model())->item(2)->setTextAlignment(Qt::AlignCenter);
    static_cast<QStandardItemModel*>(ui->mutipleSPeed->model())->item(3)->setTextAlignment(Qt::AlignCenter);

}


MainWindow::~MainWindow()
{
    player_->stop();
    preview_player_->stop();
    // player_ 的 parent 是 this，随 QObject 树自动析构，不需手动 delete
    // preview_ 的 parent 是 previewContainer_，随 ui 析构自动释放，不需手动 delete
    delete preview_player_;
    preview_player_ = nullptr;
    delete picture_;
    picture_ = nullptr;
    delete ui;
}

void MainWindow::onPlayerStateChanged()
{
    PlayerViewModel::State state = player_->state();
    if(PlayerViewModel::Running == state){
        ui->startBtn->setIcon(QIcon(":/SmartPlayer-icon/pause.png"));
        ui->progressSlider->setEnabled(true);
    }else{
        ui->startBtn->setIcon(QIcon(":/SmartPlayer-icon/start.png"));
    }
    if(state == PlayerViewModel::Stopped){
        // ui->startBtn->setEnabled(false);
        //ui->preVideoBtn->setEnabled(false);
        //ui->nextVideoBtn->setEnabled(false);
        ui->back3sBtn->setEnabled(false);
        ui->forward3sBtn->setEnabled(false);
        ui->stopBtn->setEnabled(false);
        ui->progressSlider->setEnabled(false);
        ui->volumeBtn->setEnabled(false);
        ui->volumeSlider->setEnabled(false);
        ui->mutipleSPeed->setEnabled(false);
        ui->logoBtn->setVisible(true);
        ui->speedLabel->setVisible(false);
        //ui->allTimeLabel->setText(getTimeText(0));
        ui->nowTimeLabel->setText(getTimeText(0));
        ui->progressSlider->setValue(0);
        ui->videoWidget->stop();
        // if(ui->fileList->isHidden()){
        //     on_openListBtn_clicked();
        // }
    }else{
        // ui->startBtn->setEnabled(true);
        ui->preVideoBtn->setEnabled(true);
        ui->nextVideoBtn->setEnabled(true);
        ui->back3sBtn->setEnabled(true);
        ui->forward3sBtn->setEnabled(true);
        ui->stopBtn->setEnabled(true);
        ui->progressSlider->setEnabled(true);
        ui->volumeBtn->setEnabled(true);
        ui->volumeSlider->setEnabled(true);
        ui->mutipleSPeed->setEnabled(true);
        ui->logoBtn->setVisible(false);
    }
    if(state == PlayerViewModel::Paused){
        ui->logoBtn->setVisible(true);
    }
}


void MainWindow::onPlayerTimeChanged()
{
    if(is_seeking) return;
    int64_t pos = player_->currentPos();
    ui->progressSlider->setValue(pos);
    ui->nowTimeLabel->setText(getTimeText(pos));
}

void MainWindow::onPlayerInitFinished()
{
    int duration = player_->duration();
    ui->progressSlider->setRange(0,duration);
    ui->allTimeLabel->setText(getTimeText(duration));
    if(loadingLabel_) loadingLabel_->hide();
}

void MainWindow::onPlayerPlayFailed(const QString& info)
{
    if(loadingLabel_) {
        loadingLabel_->hide();
        ui->logoBtn->show();
    }
    QMessageBox::critical(nullptr, "提示", info);
}

void MainWindow::applyPersistentSettings()
{
    // 倍速:
    player_->setSpeed(4 - ui->mutipleSPeed->currentIndex());

    // 音量
    player_->setVolume(ui->volumeSlider->value());

    // 静音状态
    player_->setMute(isMutedByUser_);
}

void MainWindow::onPlayerOpenResult(bool result)
{
    if(result){
        player_->play();

        applyPersistentSettings();

        ui->videoWidget->start();
        ui->videoWidget->setRenderSource(OpenGLRenderer::RenderSource::Video);

        if (m_summaryPanel) {
            m_summaryPanel->setVideoPath(player_->fileUrl());
        }
        if (m_transcriptPanel) {
            m_transcriptPanel->setVideoPath(player_->fileUrl());
        }
        if (m_summaryVm) {
            // VM 内部会判断状态并 stop（如有必要），无需 MainWindow 重复判断
            m_summaryVm->setVideoPath(player_->fileUrl());
        }
        // mp3渲染专辑封面
        if (!player_->hasVideo()) {
            ui->videoWidget->setRenderSource(OpenGLRenderer::RenderSource::Cover);
            QImage cover = picture_->getPreViewImage(player_->fileUrl(),ui->videoWidget->width(),ui->videoWidget->height());
            ui->videoWidget->renderCoverImage(cover);

            preview_player_->stop();
            previewContainer_->hide();
            return;
        }


        // 非视频文件关闭预览
        if (player_->mediaType() != Demuxer::MediaType::FILE_TYPE) {
            preview_player_->stop();
            previewContainer_->hide();
            return;
        }

        int ret = preview_player_->open(player_->fileUrl().toUtf8().constData());
        if (ret < 0) qDebug() << "预览初始化失败";
    }
}

void MainWindow::onVideoSizeModeChanged(int mode)
{
    ui->videoWidget->setSizeMode(mode);
}

void MainWindow::onSliderClicked(VideoSlider *slider)
{
    qDebug() << "Slider value is " << slider->value();
    // 注意：value() 返回 int，直接 * 1000000 会按 int 溢出（> ~33 分钟后变负数）
    player_->seek(qint64(slider->value()) * 1000000);
}

void MainWindow::seekRelative(int seconds)
{
    if (player_->state() == PlayerViewModel::Stopped) return;

    const int current = ui->progressSlider->value();
    const int maxVal  = ui->progressSlider->maximum();
    const int newVal  = qBound(0, current + seconds, maxVal);
    if (newVal == current) return;

    ui->progressSlider->setValue(newVal);
    player_->seek(int64_t(newVal) * 1000000);
}

void MainWindow::onSliderMouseFoucs(int seektime,int x)
{
    if (!player_->hasVideo() || player_->mediaType() != Demuxer::MediaType::FILE_TYPE) return;
    if (!preview_ || playlist_->isEmpty() || !preview_player_) return;

    preview_->clear();
    previewTimeLabel_->setText(getTimeText(seektime));
    preview_player_->requestPreview(seektime);

    QPoint globalBarPos = ui->controlBarContainer->mapToGlobal(QPoint(0, 0));
    QPoint barPosInVideoWidget = ui->videoWidget->mapFromGlobal(globalBarPos);
    QPoint globalMousePos = ui->progressSlider->mapToGlobal(QPoint(x, 0));
    QPoint videoMousePos = ui->videoWidget->mapFromGlobal(globalMousePos);

    int xPos = videoMousePos.x() - previewContainer_->width() / 2;
    int yPos = barPosInVideoWidget.y() - previewContainer_->height() - 8;
    xPos = qMax(0, qMin(xPos, ui->videoWidget->width() - previewContainer_->width()));
    yPos = qMax(0, yPos);

    previewContainer_->move(xPos, yPos);
    previewContainer_->show();
}

void MainWindow::onMouseLeaveSlider(){
    previewContainer_->hide();
}

void MainWindow::on_openFileBtn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "选择多媒体文件",
                                                    "/home",
                                                    "多媒体文件(*.mp4 *.avi *.mkv *.mp3 *.aac *.mov *.ts)");
    if(filePath.isEmpty()) return;
    addToFileList(filePath);
    scheduleSave();
    play(filePath);

    //隐藏播放列表时继续导入文件夹，则导入后打开播放列表
    if(ui->fileList->isHidden())
    {
        ui->videoWidget->resize(0.8*ui->playerPage->width(),ui->playerPage->height());
        ui->fileList->resize(0.2*ui->playerPage->width(),ui->playerPage->height());

        ui->fileList->show();
        ui->addFileBtn->show();
        ui->addDirBtn->show();
        ui->clearListBtn->show();

        ui->openListBtn->setIcon(QIcon(":/SmartPlayer-icon/right_arrow.png"));
    }
}


void MainWindow::on_startBtn_clicked()
{
    PlayerViewModel::State state = player_->state();
    if(state == PlayerViewModel::Running){
        player_->pause();
    }else if(state == PlayerViewModel::Paused){
        player_->play();
    }else if(state == PlayerViewModel::Stopped && !playlist_->isEmpty()){
        const QString cur = playlist_->currentPath();
        qDebug() << "current = " << cur;
        play(cur);
    }
}


void MainWindow::on_stopBtn_clicked()
{
    player_->stop();
    preview_player_->stop();
    previewContainer_->hide();
}


void MainWindow::on_playFinished()
{
    if (playFinished_busy_.exchange(true)) return;

    if (playlist_->isEmpty()) {
        playFinished_busy_ = false;
        player_->stop();
        return;
    }

    // 记录当前曲目的最后播放位置（用于下次恢复进度）
    const QString curPath = playlist_->currentPath();
    if (!curPath.isEmpty()) {
        ConfigManager::instance().updateVideoPosition(curPath, player_->currentPos());
    }

    // 委托 VM 计算"下一首应放谁"；VM 会 emit currentTrackRequested(path) → 我们已经
    // connect 到了 play(path)，所以这里只需调用一次。
    playlist_->advanceForFinish();

    playFinished_busy_ = false;
}


void MainWindow::on_preVideoBtn_clicked()
{
    if (playlist_->isEmpty()) {
        QMessageBox::information(nullptr, "当前列表为空", "当前列表为空，无法切换上一个视频！", QMessageBox::Yes);
        return;
    }
    ui->videoWidget->setRenderSource(OpenGLRenderer::RenderSource::None);
    if (player_->state() == PlayerViewModel::Running) {
        player_->stop();
        preview_player_->stop();
        ui->videoWidget->stop();
    }
    playlist_->previous();   // VM 会 emit currentTrackRequested → play()
}


void MainWindow::on_nextVideoBtn_clicked()
{
    if (playlist_->isEmpty()) {
        QMessageBox::information(nullptr, "当前列表为空", "当前列表为空，无法切换下一个视频！", QMessageBox::Yes);
        return;
    }
    ui->videoWidget->setRenderSource(OpenGLRenderer::RenderSource::None);
    if (player_->state() == PlayerViewModel::Running) {
        player_->stop();
        preview_player_->stop();
        ui->videoWidget->stop();
    }
    playlist_->next();
}


void MainWindow::on_switchPlayModeBtn_clicked()
{
    // VM 切换模式后会通过 playModeChanged 信号回到 View，更新按钮图标 & ToolTip
    playlist_->togglePlayMode();
}


void MainWindow::on_back3sBtn_clicked()
{
    seekRelative(-10);
}


void MainWindow::on_forward3sBtn_clicked()
{
    seekRelative(10);
}


void MainWindow::on_volumeBtn_clicked()
{
    if(player_->isMute()){
        player_->setMute(false);
        isMutedByUser_ = false;
        ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/volume.png"));
    }else{
        player_->setMute(true);
        isMutedByUser_ = true;
        ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/no_volume.png"));
    }
}



void MainWindow::on_openListBtn_clicked()
{
    if(!ui->fileList->isHidden()){
        closeFileList();
    }else{      
        openFileList();
    }


}

void MainWindow::on_fullScreenBtn_clicked()
{
    isFullScreen() ? exitFullScreenMode() : enterFullScreenMode();
}


void MainWindow::on_addFileBtn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(  this,
                                                    "选择多媒体文件", //窗口左上角显示
                                                    "/home", //初始路径
                                                    "多媒体文件 (*.mp4 *.avi *.mkv *.mp3 *.aac *.mov *.ts)"
                                                    );
    if(filePath.isEmpty()) return;

    bool wasEmpty = playlist_->isEmpty();
    addToFileList(filePath);
    scheduleSave();
    // 旧逻辑：若 listIndex == 0（即刚加进来变成 0），自动开播。语义保留为"从空列表加进第一首时自动开播"。
    if (wasEmpty) {
        play(filePath);
    }
}


void MainWindow::on_addDirBtn_clicked()
{
    QString filename = QFileDialog::getExistingDirectory(this,"选择文件夹", //窗口左上角显示
                                                         "/home" //初始路径
                                                         );
    if (filename.isEmpty()) return;

    QDir dir(filename);
    QStringList filter;
    filter << QString("*.mp4") << QString("*.avi")
           << QString("*.mkv") << QString("*.mp3")
           << QString("*.aac") << QString("*.mov")
           << QString("*.ts");
    dir.setNameFilters(filter);

    const QFileInfoList fileInfo = dir.entryInfoList(QDir::Files | QDir::CaseSensitive);
    QList<PlaylistViewModel::Track> batch;
    batch.reserve(fileInfo.size());
    for (const QFileInfo& fi : fileInfo) {
        if (playlist_->contains(fi.absoluteFilePath())) continue;
        PlaylistViewModel::Track t;
        t.path        = fi.absoluteFilePath();
        t.name        = fi.fileName();
        t.durationSec = picture_->duration(t.path);
        batch.append(t);
    }
    playlist_->addTracks(batch);
    scheduleSave();
}


void MainWindow::on_clearListBtn_clicked()
{
    playlist_->clear();
}


void MainWindow::on_mutipleSPeed_currentIndexChanged(int index)
{
    player_->setSpeed(4-index);
}

void MainWindow::onLongPressTimeout()
{
    qDebug() << "长按右方向键，进入倍速播放模式";
    player_->setSpeed(4); //长按2倍速播放
    ui->speedLabel->setVisible(true);
    ui->mutipleSPeed->setCurrentIndex(0);
    isLongPress = true; //标记为长按倍速播放
}

QString MainWindow::getTimeText(int64_t value)
{
    //获取XX：XX：XX格式的时间文本
    QLatin1Char fill = QLatin1Char('0');
    return QString("%1:%2:%3")
        .arg(value/3600,2,10,fill)
        .arg((value/60)%60,2,10,fill)
        .arg(value%60,2,10,fill);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{

}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton){
        isFullScreen() ? exitFullScreenMode() : enterFullScreenMode();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    PlayerViewModel::State state = player_->state();
    if(state != PlayerViewModel::Stopped){
        //按空格键播放和暂停
        if(event->key() == Qt::Key_Space){
            state = player_->state();
            if(state == PlayerViewModel::Running){
                player_->pause();
            }else{
                player_->play();
            }
        }

        //方向键右键，快进10秒
        else if(event->key() == Qt::Key_Right){
            state = player_->state();
            if(state != PlayerViewModel::Stopped){
                if(!event->isAutoRepeat()){
                    isLongPress = false;
                    timer.start(500);
                    // 短按立即快进；VideoSlider 已 ignore 方向键，
                    // 这里不会与 slider 内置步进叠加。
                    seekRelative(10);
                    messageInfo("快进10s",1000);
                }else{ //长按状态
                    if(!isLongPress){
                        player_->setSpeed(4);
                        ui->mutipleSPeed->setCurrentIndex(0);
                        ui->speedLabel->setVisible(true);
                        isLongPress = true;
                    }
                }
            }
        }

        //方向键左键，后退10秒
        else if(event->key() == Qt::Key_Left){
            seekRelative(-10);
            messageInfo("后退10s",1000);
        }

        //方向键上键，音量加5
        else if(event->key() == Qt::Key_Up){
            int value = ui->volumeSlider->value();
            value += 5;
            if(value > 100)
                value = 100;
            ui->volumeLabel->setText(QString("%1").arg(value));
            ui->volumeSlider->setValue(value);
            //调整音量时解除静音
            if(player_->isMute()){
                ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/volume.png"));
                player_->setMute(false);
            }

            player_->setVolume(value);
            messageInfo("音量加5",1000);
        }
        else if(event->key() == Qt::Key_Down){
            int value = ui->volumeSlider->value();
            value -= 5;
            if(value < 0)
                value = 0;
            ui->volumeLabel->setText(QString("%1").arg(value));
            ui->volumeSlider->setValue(value);

            //调整音量时解除静音
            if(player_->isMute()){
                ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/volume.png"));
                player_->setMute(false);
            }

            if(value == 0){
                ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/no_volume.png"));
                player_->setMute(true);
            }

            player_->setVolume(value);
             messageInfo("音量减5",1000);
        }
        //esc键，退出全屏
        else if(event->key() == Qt::Key_Escape){
            if(this->isFullScreen()){
                exitFullScreenMode();
            }
        }
    }
}

// void MainWindow::mouseMoveEvent(QMouseEvent *event)
// {
//     if (isFullScreenMode) {
//         hideCursorTimer->stop();
//         showControlBarAndCursor();
//     }
//     QMainWindow::mouseMoveEvent(event);
// }
void MainWindow::keyReleaseEvent(QKeyEvent* event){
    PlayerViewModel::State state = player_->state();
    if(state != PlayerViewModel::Stopped){
        if(event->key() == Qt::Key_Right){
            timer.stop();
            if(isLongPress){ //长按松开时恢复正常倍速
                player_->setSpeed(2);
                ui->mutipleSPeed->setCurrentIndex(2);
                ui->speedLabel->setVisible(false);
                isLongPress = false;
            }
            // 短按的 seekRelative(10) 已在 keyPressEvent 触发，这里不重复。
        }
    }
}

void MainWindow::contextMenuEvent(QContextMenuEvent *pEvent)
{
    // 先清空旧 action，parent 设为 popMenu，clear() 时自动 delete
    popMenu->clear();

    QAction *fileInfoAction = new QAction("  视频信息", popMenu);
    QAction *keyInfoAction = new QAction("  快捷键说明", popMenu);
    QAction *playerInfoAction = new QAction("  v1.0.0", popMenu);

    connect(keyInfoAction, &QAction::triggered, this, [this]() {
        ShotCutDialog *shotcutdialog = new ShotCutDialog(this);
        shotcutdialog->setAttribute(Qt::WA_DeleteOnClose);
        shotcutdialog->show();
    });

    connect(fileInfoAction, &QAction::triggered, this, [this](){
        VideoInfoDialog *videoinfodialog = new VideoInfoDialog(this);
        videoinfodialog->setAttribute(Qt::WA_DeleteOnClose);
        if(player_->state() != PlayerViewModel::Stopped){
            videoinfodialog->updateinformation(player_->mediaInfo());
            videoinfodialog->show();
        }else{
            QMessageBox::information(nullptr, "暂无播放视频", "打开一个视频才有信息哦！", QMessageBox::Yes);
        }
    });

    popMenu->setStyleSheet(R"(
    QMenu {
        min-width:130px;
        max-width:130px;
        background-color: rgba(0, 0, 0, 200); /* 黑色透明背景 */
        color: white; /* 文字颜色 */
        border: 1px solid rgba(255, 255, 255, 30); /* 半透明边框 */
        border-radius: 5px; /* 圆角 */
    }
    QMenu::item {
        padding: 8px 25px; /* 内边距 */
        background-color: transparent; /* 默认透明背景 */
    }
    QMenu::item:selected {
        background-color: rgba(255, 255, 255, 30); /* 选中项高亮 */
    }
    QMenu::separator {
        height: 1px;
        background: rgba(255, 255, 255, 50); /* 分隔线颜色 */
        margin: 5px 10px;
    }
)");

    // 启用透明背景和无边框
    popMenu->setWindowFlags(popMenu->windowFlags() | Qt::FramelessWindowHint);
    popMenu->setAttribute(Qt::WA_TranslucentBackground);

    popMenu->addAction(fileInfoAction);
    popMenu->addSeparator();
    popMenu->addAction(keyInfoAction);
    popMenu->addSeparator();
    popMenu->addAction(playerInfoAction);
    popMenu->exec(QCursor::pos());
    pEvent->accept();
}

void MainWindow::restoreControlBarParent()
{
    ui->controlBarContainer->setParent(ui->centralwidget);
    ui->centralwidget->layout()->addWidget(ui->controlBarContainer);
    ui->controlBarContainer->show();
}


void MainWindow::enterFullScreenMode()
{
    this->showFullScreen();
    ui->fullScreenBtn->setIcon(QIcon(":/SmartPlayer-icon/exit_full_screen.png"));

    isFullScreenMode = true;

    // 不再 setParent 到 videoWidget(QOpenGLWidget)，避免 Popup/ComboBox 被 OpenGL 表面遮挡
    // 改为从布局中取出，保持 centralwidget 为父控件，通过绝对定位悬浮在视频上方
    if (ui->centralwidget->layout()) {
        ui->centralwidget->layout()->removeWidget(ui->controlBarContainer);
    }
    ui->controlBarContainer->setParent(ui->centralwidget);
    ui->controlBarContainer->raise();
    ui->controlBarContainer->show();

    QTimer::singleShot(0, this, [=]() {
        repositionControlBarFullScreen();
    });

    // 全屏透明样式
    ui->controlBarContainer->setStyleSheet(R"(
        background-color: rgba(0, 0, 0, 80);
        border: none;
        border-radius: 0px;
    )");

    showControlBarAndCursor();
}

void MainWindow::exitFullScreenMode()
{
    showNormal();
    ui->fullScreenBtn->setIcon(QIcon(":/SmartPlayer-icon/full_screen.png"));
    isFullScreenMode = false;
    hideCursorTimer->stop();

    restoreControlBarParent();
    ui->controlBarContainer->setStyleSheet(originalControlBarStyle);

    showControlBarAndCursor();
}

bool MainWindow::event(QEvent *e)
{
#ifdef Q_OS_WIN
    // Windows DWM 限制：当窗口处于全屏状态且主窗口是 OpenGL 表面时
    // （videoWidget 是 QOpenGLWidget，进入 showFullScreen 后整个 QMainWindow
    // 都成为 OpenGL surface），其它顶层窗口（QComboBox 下拉、Qt::Popup 窗口、
    // 菜单/对话框）无法被正确合成到主窗口之上，导致全屏下点击倍速 combobox 不
    // 弹出、AI 字幕 SubtitlePopup 不显示等问题。
    //
    // 官方解决方案：给全屏窗口的 HWND 加上 WS_BORDER 标志（即使全屏也保留 1 像素
    // 边框），让 DWM 把主窗口识别为普通窗口，从而允许 popup 之类顶层窗口正确叠加。
    // 见：https://doc.qt.io/qt-6/windows-issues.html#fullscreen-opengl-based-windows
    if (e->type() == QEvent::WinIdChange) {
        if (windowHandle()) {
            HWND hwnd = reinterpret_cast<HWND>(windowHandle()->winId());
            SetWindowLongPtr(hwnd, GWL_STYLE,
                             GetWindowLongPtr(hwnd, GWL_STYLE) | WS_BORDER);
        }
    }
#endif
    return QMainWindow::event(e);
}

void MainWindow::centerLoadingLabel()
{
    if(!loadingLabel_) return;
    int x = ui->videoWidget->width() / 2 - loadingLabel_->width() / 2;
    int y = ui->videoWidget->height() / 2 - loadingLabel_->height() / 2;
    loadingLabel_->move(x, y);
}

void MainWindow::messageInfo(QString info, int interval)
{
    this->ui->infoLabel->setText(info);
    this->ui->infoLabel->show();
    QTimer::singleShot(interval, this->ui->infoLabel, &QLabel::hide);
}

void MainWindow::openFileList()
{
    ui->videoWidget->resize(0.8*ui->playerPage->width(),ui->playerPage->height());
    ui->fileList->resize(0.2*ui->playerPage->width(),ui->playerPage->height());

    ui->fileList->show();
    ui->addFileBtn->show();
    ui->addDirBtn->show();
    ui->clearListBtn->show();

    ui->openListBtn->setIcon(QIcon(":/SmartPlayer-icon/right_arrow.png"));
    centerLoadingLabel();
}

void MainWindow::closeFileList()
{
    ui->fileList->hide();
    ui->addFileBtn->hide();
    ui->addDirBtn->hide();
    ui->clearListBtn->hide();

    ui->videoWidget->resize(ui->playerPage->width(),ui->playerPage->height());
    ui->openListBtn->setIcon(QIcon(":/SmartPlayer-icon/left_arrow.png"));
    centerLoadingLabel();
}

void MainWindow::openVideoFromCommand(const QString &filePath)
{
    if(!filePath.isEmpty()){
        addToFileList(filePath);
        scheduleSave();
        play(filePath);
    }
}

void MainWindow::addToFileList(QString filePath)
{
    // 单条添加。VM 内部会判重；View 通过 trackAdded 信号自动渲染。
    PlaylistViewModel::Track t;
    t.path        = QFileInfo(filePath).absoluteFilePath();
    t.name        = QFileInfo(filePath).fileName();
    t.durationSec = picture_->duration(t.path);
    // 缩略图路径留空，UI 端 lambda 会自己生成 QImage（保留旧行为，包括"立即生成 110x65 预览图"）
    int idx = playlist_->addTrack(t);
    // 把焦点同步到这一项（如果是已存在的也要更新选中行）
    playlist_->setCurrentIndex(idx, /*emitRequest=*/false);
}

void MainWindow::play(QString filePath)
{
    if(loadingLabel_){
        centerLoadingLabel();
        loadingLabel_->show();
        ui->logoBtn->hide();
    }
    if (m_summaryPanel) {
        m_summaryPanel->setVideoPath(filePath);
    }
    if (m_summaryVm) {
        m_summaryVm->setVideoPath(filePath);
    }
    player_->open(filePath);
}


void MainWindow::on_progressSlider_valueChanged(int value)
{
    ui->nowTimeLabel->setText(getTimeText(value));
}


void MainWindow::on_volumeSlider_valueChanged(int value)
{
    ui->volumeLabel->setText(QString("%1").arg(value));

    //调整音量时解除静音
    if(player_->isMute()){
        ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/volume.png"));
        player_->setMute(false);
        isMutedByUser_ = false;
    }
    if(value == 0){
        ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/no_volume.png"));
        player_->setMute(true);
        isMutedByUser_ = true;
    }
    player_->setVolume(value);
}


void MainWindow::on_fileList_itemDoubleClicked(QListWidgetItem *item)
{
    //点击播放列表时若正在播放则清除播放帧，重新打开文件
    PlayerViewModel::State state = player_->state();
    if(state == PlayerViewModel::Running){
        player_->stop();
        //preview
        preview_player_->stop();
    }

    QString fileAbsolutePath = item->data(Qt::UserRole).toString();

    // VM 内部找索引并 emit currentIndexChanged；这里禁用 currentTrackRequested 以避免
    // 与下面 play() 重复触发。持久化当前索引由 saveAllSettings 路径完成。
    playlist_->setCurrentByPath(fileAbsolutePath, /*emitRequest=*/false);
    ConfigManager& cfg = ConfigManager::instance();
    cfg.setCurrentIndex(playlist_->currentIndex());
    cfg.save();

    play(fileAbsolutePath);
}



void MainWindow::on_rtspButton_clicked()
{
    QInputDialog dialog(this);
    dialog.setWindowTitle("请输入网络流地址");
    dialog.setLabelText("地址：");
    dialog.setStyleSheet(R"(
        QDialog{
            background-color: black;
        }
        QLabel {
            color: white;
        }
        QLineEdit {
            color: white;
            background-color: #3c3c3c;
            border: 1px solid #555;
        }
        QPushButton {
            color: white;
            background-color: #444;
            border: 1px solid #666;
            padding: 5px;
        }
        QPushButton:hover {
            background-color: #555;
        }
    )");
    QLineEdit* lineEdit = dialog.findChild<QLineEdit*>();
    if (lineEdit) {
        lineEdit->setStyleSheet("color: white;");
    }
    // 获取底部的按钮并修改文字
    QDialogButtonBox* buttonBox = dialog.findChild<QDialogButtonBox*>();
    if (buttonBox) {
        QPushButton* okBtn = buttonBox->button(QDialogButtonBox::Ok);
        QPushButton* cancelBtn = buttonBox->button(QDialogButtonBox::Cancel);
        if (okBtn) okBtn->setText("播放");
        if (cancelBtn) cancelBtn->setText("取消");
    }

    if (dialog.exec() == QDialog::Accepted) {
        QString rtsp_url = dialog.textValue();
        if(rtsp_url.isEmpty()) return;
        addToFileList(rtsp_url);
        play(rtsp_url);
    }

}


void MainWindow::on_screenShotBtn_clicked()
{
    PlayerViewModel::State state = player_->state();
    if(state == PlayerViewModel::Stopped) return;
    player_->takeScreenshot();
}


void MainWindow::on_settingBtn_clicked()
{
    settingdialog->show();
}

void MainWindow::on_setHardWare(bool on)
{
    PlayerViewModel::State state = player_->state();
    if(state == PlayerViewModel::Stopped){
        player_->useHardware(on);
        return;
    }
    player_->stop();
    preview_player_->stop();

    player_->useHardware(on);

    const QString cur = playlist_->currentPath();
    if (!cur.isEmpty()) play(cur);
}

void MainWindow::on_update_file_path(QString path)
{
    player_->setScreenshotSavePath(path);
}

void MainWindow::on_change_userDecoder(QString decoder)
{
    if(decoder.isEmpty()) return;

    PlayerViewModel::State state = player_->state();
    if(state == PlayerViewModel::Stopped){
        if(decoder == "默认(推荐)"){
            player_->setDecodeType(QString());
        }else{
            player_->setDecodeType(decoder);
        }
        return;
    }else{
        player_->stop();
        preview_player_->stop();

        if(decoder == "默认(推荐)"){
            player_->setDecodeType(QString());
        }else{
            player_->setDecodeType(decoder);
        }

        const QString cur = playlist_->currentPath();
        if (!cur.isEmpty()) play(cur);
    }
}

void MainWindow::on_modelPathChanged(const QString& path)
{
    player_->setModelPath(path);
}

void MainWindow::saveAllSettings()
{
    // 列表数据由 VM 写回；ConfigManager::save() 仍由 View 触发持久化到磁盘。
    playlist_->writeToConfig();
    ConfigManager::instance().save();
}

void MainWindow::onSaveDebounceTimeout()
{
    saveAllSettings();
}

void MainWindow::scheduleSave()
{
    if (!saveDebounceTimer_.isActive()) {
        saveDebounceTimer_.setInterval(1000);
        saveDebounceTimer_.start();
    } else {
        saveDebounceTimer_.setInterval(saveDebounceTimer_.remainingTime() + 1000);
    }
}

void MainWindow::onThumbBatchTimeout()
{
    const int BATCH = 3;
    ConfigManager& cfg = ConfigManager::instance();
    for (int i = 0; i < BATCH && thumbLoadStartIdx_ < ui->fileList->count(); ++i, ++thumbLoadStartIdx_) {
        QListWidgetItem* item = ui->fileList->item(thumbLoadStartIdx_);
        QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) continue;
        if (auto* widget = qobject_cast<VideoItemWidget*>(ui->fileList->itemWidget(item))) {
            QString thumbPath = cfg.thumbnailPathForVideo(path);
            QImage thumb;
            if (QFileInfo::exists(thumbPath)) {
                thumb.load(thumbPath);
            } else {
                thumb = picture_->getPreViewImage(path, 110, 65);
                if (!thumb.isNull()) thumb.save(thumbPath);
            }
            widget->updateThumbnail(thumb);
        }
    }
    if (thumbLoadStartIdx_ < ui->fileList->count()) {
        thumbLazyTimer_.start();
    }
}

void MainWindow::onThumbLoaded(const QString& path, const QImage& thumb)
{
    QListWidgetItem* item = findItemByPath(path);
    if (!item) return;
    if (auto* widget = qobject_cast<VideoItemWidget*>(ui->fileList->itemWidget(item))) {
        widget->updateThumbnail(thumb);
    }
}

void MainWindow::loadVideoList()
{
    // 把"过滤不存在文件"的策略保留在 View 侧（VM 不做 I/O 检查），先重写配置后再让 VM 加载。
    ConfigManager& cfg = ConfigManager::instance();
    QList<ConfigManager::VideoItem> items = cfg.getVideoList();
    if (items.isEmpty()) return;

    QList<ConfigManager::VideoItem> filtered;
    filtered.reserve(items.size());
    for (const ConfigManager::VideoItem& item : items) {
        if (QFileInfo::exists(item.path)) filtered.append(item);
    }
    if (filtered.size() != items.size()) {
        cfg.setVideoList(filtered);
        // 不立刻 save——交给 scheduleSave/saveAllSettings 路径
    }

    thumbLoadStartIdx_ = 0;
    thumbLazyTimer_.stop();

    // 触发 VM 加载（会 emit tracksReset / currentIndexChanged / playModeChanged，View 通过 connect 处理）
    playlist_->loadFromConfig();

    if (thumbLoadStartIdx_ < ui->fileList->count()) {
        thumbLazyTimer_.start();
    }
}

QListWidgetItem* MainWindow::findItemByPath(const QString& path)
{
    for (int i = 0; i < ui->fileList->count(); ++i) {
        QListWidgetItem* item = ui->fileList->item(i);
        if (item->data(Qt::UserRole).toString() == path) return item;
    }
    return nullptr;
}

void MainWindow::initPreviewWindow()
{
    previewContainer_ = new QWidget(ui->videoWidget);
    previewContainer_->setObjectName("PreviewContainer");
    previewContainer_->setStyleSheet(R"(
        #PreviewContainer {
            background-color: rgba(0, 0, 0, 160);
            border-radius: 5px;
        }
    )");
    previewContainer_->setVisible(false);
    previewContainer_->setFixedSize(180, 130);

    // 预览渲染控件
    preview_ = new OpenGLRenderer;
    preview_->setFixedSize(170, 100);
    preview_->setRenderSource(OpenGLRenderer::RenderSource::Video);

    previewTimeLabel_ = new QLabel;
    previewTimeLabel_->setFixedHeight(25);
    previewTimeLabel_->setStyleSheet(R"(
        QLabel {
            color: #f0f0f0;
            qproperty-alignment: 'AlignHCenter | AlignTop';
            background-color: transparent;
            border: none;
            padding: 1px;
        }
    )");


    QVBoxLayout *previewLayout = new QVBoxLayout(previewContainer_);
    previewLayout->addWidget(preview_, 0, Qt::AlignCenter);
    previewLayout->addWidget(previewTimeLabel_);
    previewLayout->setContentsMargins(0, 5, 0, 0);
    previewLayout->setSpacing(0);
}

void MainWindow::initControlbarPresent()
{
    hideCursorTimer = new QTimer(this);
    hideCursorTimer->setSingleShot(true);
    hideCursorTimer->setInterval(5000);
    connect(hideCursorTimer, &QTimer::timeout, this, &MainWindow::hideControlBarAndCursor);
    isFullScreenMode = false;

    originalControlBarStyle = ui->controlBarContainer->styleSheet();
}

void MainWindow::initComponent()
{
    loadingLabel_ = new QLabel(ui->videoWidget);
    loadingLabel_->setFixedSize(70, 70);
    loadingLabel_->setAlignment(Qt::AlignCenter);
    loadingLabel_->setStyleSheet("background:transparent;");
    QMovie *movie = new QMovie(":/SmartPlayer-icon/loading_4.gif");
    movie->setScaledSize(loadingLabel_->size());
    loadingLabel_->setMovie(movie);
    movie->start();
    loadingLabel_->hide();
}


void MainWindow::on_frameNv12Decoded(const QByteArray &data, int width, int height)
{
    ui->videoWidget->uploadNV12Texture(data,width,height);
}

void MainWindow::on_frameRGBADecoded(const QByteArray &data, int width, int height)
{
    ui->videoWidget->uploadRGBATexture(data,width,height);
}

void MainWindow::on_frameYuv420pDecoded(const QByteArray &data, int width, int height)
{
    ui->videoWidget->uploadYUV420PTexture(data,width,height);
}

void MainWindow::on_previewFrameDecoded(const QByteArray &data, int w, int h, AVPixelFormat fmt)
{
    if (fmt == AV_PIX_FMT_YUV420P) {
        preview_->uploadYUV420PTexture(data, w, h);
    } else if (fmt == AV_PIX_FMT_NV12) {
        preview_->uploadNV12Texture(data, w, h);
    }
}

void MainWindow::on_subtitleReady(const QString &text)
{
    ui->videoWidget->uploadSubtitleTexture(text);
}


void MainWindow::on_show_message_info(QString info, int interval)
{
    messageInfo(info,interval);
}


void MainWindow::on_fileList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (previous) {
        QWidget *prevWidget = ui->fileList->itemWidget(previous);
        if (auto videoItem = qobject_cast<VideoItemWidget *>(prevWidget)) {
            videoItem->setFileNameTextColor(Qt::white); // 恢复默认白色
        }
    }

    if (current) {
        int newIndex = ui->fileList->row(current);
        // 同步 VM 的 currentIndex（不触发"请求播放"，仅记录选中状态）
        playlist_->setCurrentIndex(newIndex, /*emitRequest=*/false);

        QWidget *currWidget = ui->fileList->itemWidget(current);
        if (auto videoItem = qobject_cast<VideoItemWidget *>(currWidget)) {
            videoItem->setFileNameTextColor(QColor(232, 88, 158)); // 设置为选中颜色
        }
    }
}


void MainWindow::on_progressSlider_sliderPressed()
{
    is_seeking = true;
}


void MainWindow::on_progressSlider_sliderReleased()
{
    is_seeking = false;
    // 注意：value() 返回 int，直接 * 1000000 会按 int 溢出（> ~33 分钟后变负数），
    // 导致 PlayerCore::seek 内 pos_us < 0 直接 return。显式转 qint64 避免溢出。
    player_->seek(qint64(ui->progressSlider->value()) * 1000000);
}

void MainWindow::on_screenshotStatus(const QString &path, bool isOk)
{
    if(isOk){
        messageInfo("截图保存成功: " + path,5000);
    }else{
        messageInfo("截图保存失败",3000);
    }
}

void MainWindow::hideControlBarAndCursor()
{
    if (!isFullScreenMode) return;
    if (m_isMouseOverControlBar) {
        hideCursorTimer->start();
        return;
    }

    ui->controlBarContainer->hide();
    this->setCursor(Qt::BlankCursor);
}

void MainWindow::showControlBarAndCursor()
{
    ui->controlBarContainer->show();
    ui->controlBarContainer->raise();
    this->unsetCursor();

    if (isFullScreenMode) hideCursorTimer->start();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!isFullScreenMode) {
        return QMainWindow::eventFilter(obj, event);
    }

    if (obj == ui->controlBarContainer) {

        if (event->type() == QEvent::Enter) {
            m_isMouseOverControlBar = true;
            hideCursorTimer->stop();
            showControlBarAndCursor();
        }

        else if (event->type() == QEvent::Leave) {
            m_isMouseOverControlBar = false;
            hideCursorTimer->start();
        }
    }

    if (obj == ui->videoWidget && event->type() == QEvent::MouseMove) {
        hideCursorTimer->stop();
        showControlBarAndCursor();
    }

    // 全屏时，fileList 或右侧 dock 显隐 / 大小变化后，同步重定位 controlBarContainer
    // 注意：dock show/hide 时 QMainWindow 的 layout 是异步更新的，
    // 必须 singleShot(0) 把重定位推迟到事件循环下一轮，否则取到的是旧 centralwidget 宽度。
    QEvent::Type evType = event->type();
    if (isFullScreenMode
        && (obj == ui->fileList || obj == ui->playerPage || obj == m_rightDock)
        && (evType == QEvent::Resize || evType == QEvent::Show || evType == QEvent::Hide)) {
        QTimer::singleShot(0, this, [this]() {
            if (isFullScreenMode) repositionControlBarFullScreen();
        });
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    centerLoadingLabel();

    // 全屏时需要重新定位控制栏（窗口大小变化时同步）
    if (isFullScreenMode) {
        repositionControlBarFullScreen();
    }
}

void MainWindow::repositionControlBarFullScreen()
{
    // 将 controlBarContainer 绝对定位到 centralwidget 的底部（覆盖在 videoWidget 上方）
    int barH = ui->controlBarContainer->height();
    int parentW = ui->centralwidget->width();
    int parentH = ui->centralwidget->height();
    ui->controlBarContainer->setGeometry(0, parentH - barH, parentW, barH);
    ui->controlBarContainer->raise();
}

// void MainWindow::on_progressSlider_sliderMoved(int position)
// {
//     player_->seek(position * 1000000);
// }


void MainWindow::on_subtitleBtn_clicked()
{

    if (!subtitlePopup_) {
        subtitlePopup_ = new SubtitlePopup(this);

        // 从配置加载字幕字体大小并设置到渲染器
        int savedFontSize = ConfigManager::instance().getSubtitleFontSize();
        subtitlePopup_->fontSizeSlider_->setValue(savedFontSize);
        ui->videoWidget->setSubtitleFontSize(savedFontSize);

        connect(subtitlePopup_->realtimeBtn_, &QPushButton::toggled, this, [this](bool on){
            qDebug() << "实时字幕:" << on;
            player_->setAsrEnabled(on);
        });

        connect(subtitlePopup_->translateBtn_, &QPushButton::toggled, this, [](bool on){
            qDebug() << "中英翻译:" << on;
        });

        connect(subtitlePopup_->fontSizeSlider_, &QSlider::valueChanged, this, [this](int val){
            ui->videoWidget->setSubtitleFontSize(val);
            ConfigManager::instance().setSubtitleFontSize(val);
        });
    }

    subtitlePopup_->adjustSize();

    QPoint btnGlobal = ui->subtitleBtn->mapToGlobal(QPoint(0, 0));

    int btnCenterX = btnGlobal.x() + ui->subtitleBtn->width() / 2;
    int popupX = btnCenterX - subtitlePopup_->width() / 2;

    int popupY = btnGlobal.y() - subtitlePopup_->height() - 8;

    subtitlePopup_->move(popupX, popupY);
    subtitlePopup_->show();
}

void MainWindow::setupRightPanel() {
    // MVVM 阶段 3a：用 SummaryViewModel 替换原始的 VideoSummaryManager。
    // VM 内部拥有 manager，Panel 通过 VM 与之交互，MainWindow 不再持有原始 Model。
    m_summaryVm       = new SummaryViewModel(this);
    // MVVM 阶段 3b：TranscriptPanel 业务数据迁移到 TranscriptViewModel
    m_transcriptVm    = new TranscriptViewModel(this);
    m_summaryPanel    = new SummaryPanel(this);
    m_transcriptPanel = new TranscriptPanel(this);
    m_transcriptPanel->bindViewModel(m_transcriptVm);

    // 用 SlidingTabWidget 把"AI 视频总结"和"视频文稿"装到 mainwindow 右侧边缘，
    // 两个 tab 按钮等宽平分整个面板宽度，切换时有"胶囊背景色块"在按钮之间滑动的动画。
    // 不再包一层 QDockWidget——避免 dock 标题和 tab 标题重复。
    m_rightTab = new SlidingTabWidget(this);
    m_rightTab->setAnimationDuration(200); // 用户偏好：200ms
    m_rightTab->addTab(m_summaryPanel,    QStringLiteral(u"AI \u89c6\u9891\u603b\u7ed3"));
    m_rightTab->addTab(m_transcriptPanel, QStringLiteral(u"\u89c6\u9891\u6587\u7a3f"));
    m_rightTab->setMinimumWidth(320);
    m_rightTab->setMaximumWidth(480);
    m_rightTab->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // 通过 QMainWindow::addDockWidget 把它放到右侧。
    // 为了拿到 QDockWidget 接口（控制显隐/停靠），临时包一个"薄"的 dock（无标题）。
    m_rightDock = new QDockWidget(this);
    m_rightDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    m_rightDock->setFeatures(QDockWidget::NoDockWidgetFeatures);  // 隐藏标题栏/关闭按钮
    m_rightDock->setTitleBarWidget(new QWidget());  // 用空 widget 顶掉标题栏
    m_rightDock->setWidget(m_rightTab);
    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);
    m_rightDock->installEventFilter(this);  // 全屏下监听 dock 显隐后同步控制栏宽度
    m_rightDock->hide();

    // SummaryPanel 数据绑定（通过 ViewModel）
    m_summaryPanel->bindViewModel(m_summaryVm);
    connect(m_summaryPanel, &SummaryPanel::seekTo, this, [this](qint64 ms) {
        player_->seek(ms);
    });
    connect(player_, &PlayerViewModel::timeChanged, m_summaryPanel, [this]() {
        m_summaryPanel->onPositionChanged(player_->currentPos());
    });

    // TranscriptPanel 信号连接
    connect(m_transcriptPanel, &TranscriptPanel::seekTo, this, [this](qint64 ms) {
        // 文稿面板传的是真·毫秒；PlayerViewModel::seek 内部用 AV_TIME_BASE_Q
        // 转发给 demuxer（微秒），这里 × 1000 把毫秒转成微秒。
        player_->seek(ms * 1000);
    });
    connect(player_, &PlayerViewModel::timeChanged, m_transcriptPanel, [this]() {
        m_transcriptPanel->onPositionChanged(player_->currentPos());
    });
    connect(player_, &PlayerViewModel::initFinished, this, [this]() {
        m_transcriptPanel->setDuration(player_->duration());
    });

    // MVVM 阶段 3a：SummaryViewModel ↔ TranscriptPanel 的数据流由 MainWindow 协调
    //   - ASR 完成 → 文稿面板获得字幕（流式）
    //   - 结构化 Report → 章节 / 段落 / 字幕 一并喂给文稿面板
    connect(m_summaryVm, &SummaryViewModel::asrCompleted,
            m_transcriptPanel, &TranscriptPanel::setSubtitleItems);
    connect(m_summaryVm, &SummaryViewModel::reportChanged, this, [this]() {
        if (!m_summaryVm->hasReport() || !m_transcriptPanel) return;
        const SummaryReport& r = m_summaryVm->report();
        m_transcriptPanel->setChapters(r.chapters);
        m_transcriptPanel->setSegments(r.segments);
        m_transcriptPanel->setSubtitleItems(r.asrResults);
    });

    // 不再使用 menubar——面板切换由 tab 直接承担
    if (QMenuBar* mb = menuBar()) {
        mb->setVisible(false);
    }
}

void MainWindow::on_aiSummaryBtn_clicked()
{
    if (!m_rightDock) return;
    m_rightDock->setVisible(!m_rightDock->isVisible());
}
