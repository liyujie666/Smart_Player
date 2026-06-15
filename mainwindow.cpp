#include "mainwindow.h"
#include "ui_mainwindow.h"
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
#include <QTabWidget>
#include <QAction>
#include <QMenu>
#include <QMenuBar>

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
    setContentsMargins(0, 0, 0, 0);
    settingdialog = new settingDialog(this);
    popMenu = new QMenu(this);
    picture_ = new PictureCreator();

    initPreviewWindow();
    initControlbarPresent();
    initComponent();

    player_ = new PlayerCore(this);
    preview_player_ = new PreviewPlayer();

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
    connect(player_, &PlayerCore::openResult,this,&MainWindow::onPlayerOpenResult);//视频渲染
    connect(player_, &PlayerCore::stateChanged,this,&MainWindow::onPlayerStateChanged); //播放器状态转换
    connect(player_,&PlayerCore::timeChanged,this,&MainWindow::onPlayerTimeChanged); //更新当前播放时间
    connect(player_,&PlayerCore::initFinished,this,&MainWindow::onPlayerInitFinished); //初始化播放器参数
    connect(player_,&PlayerCore::playFailed,this,&MainWindow::onPlayerPlayFailed); //播放出错
    connect(player_,&PlayerCore::screecshotStatus,this,&MainWindow::on_screenshotStatus);
    connect(player_, &PlayerCore::playFinished, this, &MainWindow::on_playFinished);
    connect(player_,&PlayerCore::frameYuv420pDecoded,this,&MainWindow::on_frameYuv420pDecoded);//视频渲染
    connect(player_,&PlayerCore::frameNv12Decoded,this,&MainWindow::on_frameNv12Decoded);//视频渲染
    connect(player_,&PlayerCore::frameRGBADecoded,this,&MainWindow::on_frameRGBADecoded);//视频渲染
    connect(player_, &PlayerCore::subtitleReady,this, &MainWindow::on_subtitleReady);
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
    //释放线程
    player_->stop();
    preview_player_->stop();
    //释放资源
    delete ui;
    delete player_;
    delete preview_player_;
    delete picture_;
    delete preview_;

    player_ = nullptr;
    preview_player_ = nullptr;
    preview_ = nullptr;
    picture_ = nullptr;
}

void MainWindow::onPlayerStateChanged()
{
    PlayerCore::State state = player_->getState();
    if(PlayerCore::Running == state){
        ui->startBtn->setIcon(QIcon(":/SmartPlayer-icon/pause.png"));
        ui->progressSlider->setEnabled(true);
    }else{
        ui->startBtn->setIcon(QIcon(":/SmartPlayer-icon/start.png"));
    }
    if(state == PlayerCore::Stopped){
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
    if(state == PlayerCore::Paused){
        ui->logoBtn->setVisible(true);
    }
}


void MainWindow::onPlayerTimeChanged()
{
    if(is_seeking) return;
    int64_t pos = player_->getCurrentPos();
    ui->progressSlider->setValue(pos);
    ui->nowTimeLabel->setText(getTimeText(pos));
}

void MainWindow::onPlayerInitFinished()
{
    int duration = player_->getDuration();
    qDebug() << "duration" << duration;
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

void MainWindow::onPlayerOpenResult(bool result)
{
    if(result){
        player_->play();
        ui->videoWidget->start();
        ui->videoWidget->setRenderSource(OpenGLRenderer::RenderSource::Video);

        if (m_summaryPanel) {
            m_summaryPanel->setVideoPath(player_->getFileUrl());
        }
        if (m_transcriptPanel) {
            m_transcriptPanel->setVideoPath(player_->getFileUrl());
        }
        if (m_summaryManager && m_summaryManager->state() != SummaryState::Idle
            && m_summaryManager->state() != SummaryState::Finished
            && m_summaryManager->state() != SummaryState::Error) {
            m_summaryManager->stopSummary();
        }
        // mp3渲染专辑封面
        if (!player_->hasVideo()) {
            ui->videoWidget->setRenderSource(OpenGLRenderer::RenderSource::Cover);
            QImage cover = picture_->getPreViewImage(player_->getFileUrl(),ui->videoWidget->width(),ui->videoWidget->height());
            ui->videoWidget->renderCoverImage(cover);

            preview_player_->stop();
            previewContainer_->hide();
            return;
        }


        // 非视频文件关闭预览
        if (player_->getMediaType() != Demuxer::MediaType::FILE_TYPE) {
            preview_player_->stop();
            previewContainer_->hide();
            return;
        }

        int ret = preview_player_->open(player_->getFileUrl().toUtf8().constData());
        if (ret < 0) qDebug() << "预览初始化失败";
    }
}

void MainWindow::onVideoSizeModeChanged(int mode)
{
    ui->videoWidget->setSizeMode(mode);
}

void MainWindow::onSliderClicked(VideoSlider *slider)
{
    player_->seek(slider->value() * 1000000);
}

void MainWindow::onSliderMouseFoucs(int seektime,int x)
{
    if (!player_->hasVideo() || player_->getMediaType() != Demuxer::MediaType::FILE_TYPE) return;
    if (!preview_ || fileList.isEmpty() || !preview_player_) return;

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
    if(filePath == nullptr) return;
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
    PlayerCore::State state = player_->getState();
    if(state == PlayerCore::Running){
        player_->pause();
    }else if(state == PlayerCore::Paused){
        player_->play();
    }else if(state == PlayerCore::Stopped && !fileList.empty()){
        qDebug() << "fileList[listIndex] = " << fileList[listIndex];
        play(fileList[listIndex]);
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

    if (fileList.isEmpty()) {
        playFinished_busy_ = false;
        player_->stop();
        return;
    }
    if (listIndex < 0 || listIndex >= fileList.size()) {
        listIndex = 0;
    }
    ConfigManager::instance().updateVideoPosition(fileList[listIndex], player_->getCurrentPos());
    switch (playMode_) {
    case PlayMode::ListLoop: {
        int next = listIndex + 1;
        if (next >= fileList.count()) next = 0;
        listIndex = next;
        ui->fileList->setCurrentRow(listIndex);
        play(fileList.at(listIndex));
        break;
    }
    case PlayMode::SingleRepeat:
        ui->fileList->setCurrentRow(listIndex);
        play(fileList.at(listIndex));
        break;
    case PlayMode::Shuffle: {
        if (shuffledList_.isEmpty()) {
            playFinished_busy_ = false;
            player_->stop();
            return;
        }
        static int shuffleIdx = 0;
        shuffleIdx++;
        if (shuffleIdx >= shuffledList_.count()) {
            shuffleIdx = 0;
            //std::random_shuffle(shuffledList_.begin(), shuffledList_.end());
            std::shuffle(shuffledList_.begin(), shuffledList_.end(), std::mt19937(std::random_device{}()));
        }
        int idx = fileList.indexOf(shuffledList_.at(shuffleIdx));
        if (idx >= 0) listIndex = idx;
        ui->fileList->setCurrentRow(listIndex);
        play(fileList.at(listIndex));
        break;
    }
    }

    playFinished_busy_ = false;
}


void MainWindow::on_preVideoBtn_clicked()
{
    if(fileList.count() == 0){
        QMessageBox::information(NULL,"当前列表为空","当前列表为空，无法切换上一个视频！",QMessageBox::Yes);
        return;
    }
    //先关闭
    ui->videoWidget->setRenderSource(OpenGLRenderer::RenderSource::None);
    PlayerCore::State state = player_->getState();
    if(state == PlayerCore::Running){
        player_->stop();
        preview_player_->stop();
        ui->videoWidget->stop();
    }

    listIndex -= 1;
    //如果列表中就当前一个视频
    if(listIndex < 0){
        listIndex = fileList.count() - 1;
    }
    ui->fileList->setCurrentRow(listIndex);
    play(fileList[listIndex]);
}


void MainWindow::on_nextVideoBtn_clicked()
{
    if(fileList.count() == 0){
        QMessageBox::information(NULL,"当前列表为空","当前列表为空，无法切换下一个视频！",QMessageBox::Yes);
        return;
    }
    //先关闭
    ui->videoWidget->setRenderSource(OpenGLRenderer::RenderSource::None);
    PlayerCore::State state = player_->getState();
    if(state == PlayerCore::Running){
        player_->stop();
        preview_player_->stop();
        ui->videoWidget->stop();
    }

    listIndex += 1;
    //如果列表中就当前一个视频
    if(listIndex == fileList.count()){
        listIndex = 0;
    }
    ui->fileList->setCurrentRow(listIndex);
    play(fileList[listIndex]);
}


void MainWindow::on_switchPlayModeBtn_clicked()
{
    switch (playMode_) {
    case PlayMode::ListLoop:
        playMode_ = PlayMode::SingleRepeat;
        ui->switchPlayModeBtn->setIcon(QIcon(":/SmartPlayer-icon/single_circle.png"));
        ui->switchPlayModeBtn->setToolTip(QString::fromUtf8("单曲循环"));
        break;
    case PlayMode::SingleRepeat:
        playMode_ = PlayMode::Shuffle;
        ui->switchPlayModeBtn->setIcon(QIcon(":/SmartPlayer-icon/random_circle.png"));
        ui->switchPlayModeBtn->setToolTip(QString::fromUtf8("随机播放"));
        if (!fileList.isEmpty()) {
            shuffledList_ = fileList;
            //std::random_shuffle(shuffledList_.begin(), shuffledList_.end());
            std::shuffle(shuffledList_.begin(), shuffledList_.end(), std::mt19937(std::random_device{}()));
        }
        break;
    case PlayMode::Shuffle:
        playMode_ = PlayMode::ListLoop;
        ui->switchPlayModeBtn->setIcon(QIcon(":/SmartPlayer-icon/list_circle.png"));
        ui->switchPlayModeBtn->setToolTip(QString::fromUtf8("列表循环"));
        break;
    }
}


void MainWindow::on_back3sBtn_clicked()
{
    PlayerCore::State state = player_->getState();
    if(state != PlayerCore::Stopped){
        ui->progressSlider->changeValue(-10);
    }
}


void MainWindow::on_forward3sBtn_clicked()
{
    PlayerCore::State state = player_->getState();
    if(state != PlayerCore::Stopped){
        ui->progressSlider->changeValue(10);
    }
}


void MainWindow::on_volumeBtn_clicked()
{
    if(player_->isMute()){
        player_->setMute(false);
        ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/volume.png"));
    }else{
        player_->setMute(true);
        ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/no_volume.png"));
    }
}



void MainWindow::on_openListBtn_clicked()
{
    if(!ui->fileList->isHidden()){ //视频列表为打开状态
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
    if(filePath==nullptr) return;//没有成功打开文件
    addToFileList(filePath);
    scheduleSave();
    if(listIndex == 0){
        play(filePath);
    }
}


void MainWindow::on_addDirBtn_clicked()
{
    QString filename = QFileDialog::getExistingDirectory(this,"选择文件夹", //窗口左上角显示
                                                         "/home" //初始路径
                                                         );
    QDir *dir = new QDir(filename);
    QStringList filter;
    filter << QString("*.mp4") << QString("*.avi")
           << QString("*.mkv") << QString("*.mp3")
           << QString("*.aac") << QString("*.mov")
           << QString("*.ts");
    dir->setNameFilters(filter);

    QList fileInfo = dir->entryInfoList(QDir::Files | QDir::CaseSensitive);//过滤条件为只限文件并区分大小写
    for(int i=0;i < fileInfo.count();i++){
        if(!fileList.contains(fileInfo.at(i).absoluteFilePath())){  //不与已有文件重复的情况下
            int dur = picture_->getDuration(fileInfo.at(i).absoluteFilePath());
            fileDurationList.append(dur);
            VideoItemWidget *itemWidget = new VideoItemWidget(picture_->getPreViewImage(fileInfo.at(i).absoluteFilePath(),110,65), fileInfo.at(i).fileName(), getTimeText(dur));
            QListWidgetItem *item = new QListWidgetItem(ui->fileList);

            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            item->setBackground(Qt::transparent); // 不设置颜色，完全透明
            item->setSizeHint(itemWidget->sizeHint());  // 让 item 适应 widget 大小
            item->setData(Qt::UserRole, fileInfo.at(i).absoluteFilePath());      // 存储完整路径

            // 5. 加入到 QListWidget 中
            ui->fileList->addItem(item);
            ui->fileList->setItemWidget(item, itemWidget);

            // 6. 记录路径
            fileList.append(fileInfo.at(i).absoluteFilePath());
        }
    }
    scheduleSave();
    delete dir;
}


void MainWindow::on_clearListBtn_clicked()
{
    ui->fileList->clear();
    fileList.clear();
    fileDurationList.clear();
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
    PlayerCore::State state = player_->getState();
    if(state != PlayerCore::Stopped){
        //按空格键播放和暂停
        if(event->key() == Qt::Key_Space){
            state = player_->getState();
            if(state == PlayerCore::Running){
                player_->pause();
            }else{
                player_->play();
            }
        }

        //方向键右键，快进15秒
        else if(event->key() == Qt::Key_Right){
            state = player_->getState();
            if(state != PlayerCore::Stopped){
                if(!event->isAutoRepeat()){
                    isLongPress = false;
                    timer.start(500);
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
            state = player_->getState();
            if(state != PlayerCore::Stopped){
                ui->progressSlider->changeValue(-10);
                messageInfo("后退10s",1000);
            }
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
    PlayerCore::State state = player_->getState();
    if(state != PlayerCore::Stopped){
        if(event->key() == Qt::Key_Right){
            timer.stop();
            if(!isLongPress){ //如果是短按
                 ui->progressSlider->changeValue(10); //快进10s
                 messageInfo("快进10s",1000);
            }else{
                player_->setSpeed(2);
                ui->mutipleSPeed->setCurrentIndex(2);
                ui->speedLabel->setVisible(false);
                isLongPress = false;
            }
        }
    }
}

void MainWindow::contextMenuEvent(QContextMenuEvent *pEvent)
{

    QAction *fileInfoAction = new QAction("  视频信息",this);
    QAction *keyInfoAction = new QAction("  快捷键说明",this);
    QAction *playerInfoAction = new QAction("  v1.0.0",this);

    connect(keyInfoAction, &QAction::triggered, this, [this]() {
        ShotCutDialog *shotcutdialog = new ShotCutDialog(this);
        shotcutdialog->show();
    });


    connect(fileInfoAction,&QAction::triggered,this,[this](){
        VideoInfoDialog *videoinfodialog = new VideoInfoDialog(this);
        if(player_->getState() != PlayerCore::Stopped){
            videoinfodialog->updateinformation(player_->getAVFormatContext(),player_->getFileUrl().toStdString().c_str());
            videoinfodialog->show();
        }else{
            QMessageBox::information(NULL,"暂无播放视频","打开一个视频才有信息哦！",QMessageBox::Yes);
        }

    });


    popMenu->clear();
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

    ui->controlBarContainer->setParent(ui->videoWidget);
    ui->controlBarContainer->raise();

    QTimer::singleShot(0, this, [=]() {
        ui->controlBarContainer->setGeometry(
            0,
            // 强制贴紧视频最底部
            ui->videoWidget->height() - ui->controlBarContainer->height(),
            ui->videoWidget->width(),
            ui->controlBarContainer->height()
            );
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
    if(!fileList.contains(filePath)){  //判断视频列表是否有当前视频文件
        QFileInfo temp(filePath);
        //qDebug() << picture_->getDuration();
        int dur = picture_->getDuration(filePath);
        fileDurationList.append(dur);
        QImage image = picture_->getPreViewImage(filePath,110,65);
        VideoItemWidget *itemWidget = new VideoItemWidget(image, temp.fileName(), getTimeText(dur));
        QListWidgetItem *item = new QListWidgetItem(ui->fileList);

        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        // item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled); // 清除多余属性
        item->setBackground(Qt::transparent); // 不设置颜色，完全透明
        item->setSizeHint(itemWidget->sizeHint());  // 让 item 适应 widget 大小
        item->setData(Qt::UserRole, temp.absoluteFilePath());      // 存储完整路径

        // 5. 加入到 QListWidget 中
        ui->fileList->addItem(item);
        ui->fileList->setItemWidget(item, itemWidget);

        // 6. 记录路径
        fileList.append(filePath);
    }

    ui->fileList->update();
    for(int i =0;i < fileList.size();i++){
        if(filePath == fileList[i]){
            listIndex = i;
        }
    }
    ui->fileList->setCurrentRow(listIndex);
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
    if (m_summaryManager && m_summaryManager->state() != SummaryState::Idle
        && m_summaryManager->state() != SummaryState::Finished
        && m_summaryManager->state() != SummaryState::Error) {
        m_summaryManager->stopSummary();
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
    }
    if(value == 0){
        ui->volumeBtn->setIcon(QIcon(":/SmartPlayer-icon/no_volume.png"));
        player_->setMute(true);
    }
    player_->setVolume(value);
}


void MainWindow::on_fileList_itemDoubleClicked(QListWidgetItem *item)
{
    //点击播放列表时若正在播放则清除播放帧，重新打开文件
    PlayerCore::State state = player_->getState();
    if(state == PlayerCore::Running){
        player_->stop();
        //preview
        preview_player_->stop();
    }

    QString fileAbsolutePath = item->data(Qt::UserRole).toString();

    ConfigManager& cfg = ConfigManager::instance();
    for(int i=0;i<fileList.count();i++)
    {
        if(fileAbsolutePath == fileList[i]){
            listIndex = i;
            cfg.setCurrentIndex(listIndex);
            cfg.save();
        }
    }

    play(fileList[listIndex]);
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
        if(rtsp_url == nullptr) return;
        addToFileList(rtsp_url);
        play(rtsp_url);
    }

}


void MainWindow::on_screenShotBtn_clicked()
{
    PlayerCore::State state = player_->getState();
    if(state == PlayerCore::Stopped) return;
    player_->takeScreenshot();
}


void MainWindow::on_settingBtn_clicked()
{
    settingdialog->show();
}

void MainWindow::on_setHardWare(bool on)
{
    PlayerCore::State state = player_->getState();
    if(state == PlayerCore::Stopped){
        player_->useHardware(on);
        return;
    }
    player_->stop();
    preview_player_->stop();

    player_->useHardware(on);

    play(fileList[listIndex]);

}

void MainWindow::on_update_file_path(QString path)
{
    player_->setScreenshotSavePath(path);
}

void MainWindow::on_change_userDecoder(QString decoder)
{
    if(decoder.isEmpty()) return;

    PlayerCore::State state = player_->getState();
    if(state == PlayerCore::Stopped){
        if(decoder == "默认(推荐)"){
            player_->setDecodeType(NULL);
        }else{
            player_->setDecodeType(decoder);
        }
        return;
    }else{
        player_->stop();
        preview_player_->stop();

        if(decoder == "默认(推荐)"){
            player_->setDecodeType(NULL);
        }else{
            player_->setDecodeType(decoder);
        }

        play(fileList[listIndex]);
    }
}

void MainWindow::on_modelPathChanged(const QString& path)
{
    player_->setModelPath(path);
}

void MainWindow::saveAllSettings()
{
    ConfigManager& cfg = ConfigManager::instance();
    cfg.setPlayMode(playMode_);

    QList<ConfigManager::VideoItem> items;
    for (int i = 0; i < fileList.size(); ++i) {
        ConfigManager::VideoItem item;
        item.path = fileList[i];
        item.name = QFileInfo(fileList[i]).fileName();
        item.duration = (i < fileDurationList.size()) ? fileDurationList[i] : 0;
        item.thumbnail = cfg.thumbnailPathForVideo(fileList[i]);
        items.append(item);
    }
    cfg.setVideoList(items);
    cfg.setCurrentIndex(listIndex);
    cfg.save();
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
    ConfigManager& cfg = ConfigManager::instance();
    QList<ConfigManager::VideoItem> items = cfg.getVideoList();
    if (items.isEmpty()) return;

    thumbLoadStartIdx_ = 0;
    thumbLazyTimer_.stop();

    for (const ConfigManager::VideoItem& item : items) {
        if (!QFileInfo::exists(item.path)) continue;

        QFileInfo temp(item.path);
        QImage image;
        if (!item.thumbnail.isEmpty() && QFileInfo::exists(item.thumbnail)) {
            image.load(item.thumbnail);
        } else {
            image = QImage(110, 65, QImage::Format_ARGB32);
            image.fill(Qt::darkGray);
        }
        VideoItemWidget *itemWidget = new VideoItemWidget(
            image, temp.fileName(),
            getTimeText(item.duration > 0 ? item.duration : picture_->getDuration(item.path)));
        QListWidgetItem *listItem = new QListWidgetItem(ui->fileList);
        listItem->setFlags(listItem->flags() & ~Qt::ItemIsSelectable);
        listItem->setBackground(Qt::transparent);
        listItem->setSizeHint(itemWidget->sizeHint());
        listItem->setData(Qt::UserRole, temp.absoluteFilePath());
        ui->fileList->addItem(listItem);
        ui->fileList->setItemWidget(listItem, itemWidget);
        fileList.append(item.path);
        fileDurationList.append(item.duration);
    }

    if (thumbLoadStartIdx_ < ui->fileList->count()) {
        thumbLazyTimer_.start();
    }

    int savedIndex = cfg.getCurrentIndex();
    qDebug() << "index " << savedIndex;
    if (savedIndex >= 0 && savedIndex < fileList.size()) {
        listIndex = savedIndex;
        ui->fileList->setCurrentRow(listIndex);
    }

    PlayMode mode = cfg.getPlayMode();
    switch (mode) {
    case PlayMode::SingleRepeat:
        playMode_ = PlayMode::SingleRepeat;
        ui->switchPlayModeBtn->setIcon(QIcon(":/SmartPlayer-icon/single_circle.png"));
        ui->switchPlayModeBtn->setToolTip(QString::fromUtf8("单曲循环"));
        break;
    case PlayMode::Shuffle:
        playMode_ = PlayMode::Shuffle;
        ui->switchPlayModeBtn->setIcon(QIcon(":/SmartPlayer-icon/random_circle.png"));
        ui->switchPlayModeBtn->setToolTip(QString::fromUtf8("随机播放"));
        if (!fileList.isEmpty()) {
            shuffledList_ = fileList;
            //std::random_shuffle(shuffledList_.begin(), shuffledList_.end());
            std::shuffle(shuffledList_.begin(), shuffledList_.end(), std::mt19937(std::random_device{}()));
        }
        break;
    default:
        playMode_ = PlayMode::ListLoop;
        ui->switchPlayModeBtn->setIcon(QIcon(":/SmartPlayer-icon/list_circle.png"));
        ui->switchPlayModeBtn->setToolTip(QString::fromUtf8("列表循环"));
        break;
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
            font-size: 12px;
            font-family: Microsoft YaHei;
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
    hideCursorTimer->setSingleShot(true);   // 单次触发
    hideCursorTimer->setInterval(5000);     // 3秒超时
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
        listIndex = newIndex;

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
    player_->seek(ui->progressSlider->value() * 1000000);
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

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    centerLoadingLabel();
}

// void MainWindow::on_progressSlider_sliderMoved(int position)
// {
//     player_->seek(position * 1000000);
// }


void MainWindow::on_subtitleBtn_clicked()
{

    if (!subtitlePopup_) {
        subtitlePopup_ = new SubtitlePopup(this);
        connect(subtitlePopup_->realtimeBtn_, &QPushButton::toggled, this, [this](bool on){
            qDebug() << "实时字幕:" << on;
            player_->setAsrEnabled(on);
        });

        connect(subtitlePopup_->translateBtn_, &QPushButton::toggled, this, [](bool on){
            qDebug() << "中英翻译:" << on;
        });
    }

    subtitlePopup_->adjustSize();  // 很关键

    QPoint btnGlobal = ui->subtitleBtn->mapToGlobal(QPoint(0, 0));

    int btnCenterX = btnGlobal.x() + ui->subtitleBtn->width() / 2;
    int popupX = btnCenterX - subtitlePopup_->width() / 2;

    int popupY = btnGlobal.y() - subtitlePopup_->height() - 8;

    subtitlePopup_->move(popupX, popupY);
    subtitlePopup_->show();
}

void MainWindow::setupRightPanel() {
    m_summaryManager  = new VideoSummaryManager();
    m_summaryPanel    = new SummaryPanel(this);
    m_transcriptPanel = new TranscriptPanel(this);

    // 用 QTabWidget 把"AI 视频总结"和"视频文稿"装到 mainwindow 右侧边缘，
    // 不再包一层 QDockWidget——避免 dock 标题和 tab 标题重复。
    // 行为类似浏览器的标签页：点 tab 切换下方内容。
    m_rightTab = new QTabWidget(this);
    m_rightTab->setDocumentMode(true);
    m_rightTab->setTabsClosable(false);
    m_rightTab->setMovable(false);
    m_rightTab->setUsesScrollButtons(true);
    m_rightTab->addTab(m_summaryPanel,    QStringLiteral(u"AI \u89c6\u9891\u603b\u7ed3"));
    m_rightTab->addTab(m_transcriptPanel, QStringLiteral(u"\u89c6\u9891\u6587\u7a3f"));
    m_rightTab->setMinimumWidth(320);
    m_rightTab->setMaximumWidth(480);
    m_rightTab->setStyleSheet(QString::fromLatin1(
        "QTabWidget::pane {"
            " border: 1px solid #E5E7EB; border-top: none;"
            " background: #FFFFFF;"
        "}"
        "QTabBar::tab {"
            " height: 28px; padding: 0 14px;"
            " background: #F3F4F6; color: #6B7280;"
            " border: 1px solid #E5E7EB; border-bottom: none;"
            " border-top-left-radius: 6px; border-top-right-radius: 6px;"
            " margin-right: 2px; font-size: 12px; font-family: Microsoft YaHei, sans-serif;"
        "}"
        "QTabBar::tab:selected {"
            " background: #FFFFFF; color: #0C4A6E; font-weight: bold;"
            " border-bottom: 1px solid #FFFFFF;"
        "}"
        "QTabBar::tab:hover:!selected { background: #E0F2FE; color: #0C4A6E; }"
    ));
    m_rightTab->hide();

    // 通过 QMainWindow::addDockWidget 把它放到右侧。
    // 为了拿到 QDockWidget 接口（控制显隐/停靠），临时包一个"薄"的 dock（无标题）。
    m_rightDock = new QDockWidget(this);
    m_rightDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    m_rightDock->setFeatures(QDockWidget::NoDockWidgetFeatures);  // 隐藏标题栏/关闭按钮
    m_rightDock->setTitleBarWidget(new QWidget());  // 用空 widget 顶掉标题栏
    m_rightDock->setWidget(m_rightTab);
    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);
    m_rightDock->hide();

    // SummaryPanel 数据绑定
    m_summaryPanel->bindManager(m_summaryManager);
    connect(m_summaryPanel, &SummaryPanel::seekTo, this, [this](qint64 ms) {
        player_->seek(ms);
    });
    connect(player_, &PlayerCore::timeChanged, m_summaryPanel, [this]() {
        m_summaryPanel->onPositionChanged(player_->getCurrentPos());
    });

    // TranscriptPanel 信号连接
    connect(m_transcriptPanel, &TranscriptPanel::seekTo, this, [this](qint64 ms) {
        // 文稿面板传的是真·毫秒；PlayerCore::seek 内部用 AV_TIME_BASE_Q
        // 转发给 demuxer（微秒），这里 × 1000 把毫秒转成微秒。
        player_->seek(ms * 1000);
    });
    connect(player_, &PlayerCore::timeChanged, m_transcriptPanel, [this]() {
        m_transcriptPanel->onPositionChanged(player_->getCurrentPos());
    });
    connect(player_, &PlayerCore::initFinished, this, [this]() {
        m_transcriptPanel->setDuration(player_->getDuration());
    });
    connect(m_summaryManager, &VideoSummaryManager::asrCompleted,
            m_transcriptPanel, &TranscriptPanel::setSubtitleItems);
    m_summaryPanel->setTranscriptPanel(m_transcriptPanel);

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
