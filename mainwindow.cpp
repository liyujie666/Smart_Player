#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
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
#include <QtConcurrent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)

{

    ui->setupUi(this);
    this->setWindowTitle("Smart_Player");
    this->setWindowIcon(QIcon(":/SmartPlayer-icon/logo.png"));
    setContentsMargins(0, 0, 0, 0);
    settingdialog = new settingDialog(ui->videoWidget,this);
    popMenu = new QMenu(this);
    picture_ = new PictureCreator();
    //初始化预览窗口VideoWidget对象
    preview_ = nullptr;
    preview_ = new VideoWidget(this);
    preview_->resize(160,90);
    preview_->move(0, 0);
    preview_->setVisible(false);
    preview_->setStyleSheet("background:transparent;border-radius:5px;"); // 透明背景
    preview_->setParent(ui->videoWidget);

    ui->videoWidget->setControlBar(ui->controlBarContainer);

    qRegisterMetaType<VideoPlayer::VideoSwsSpec>("VideoSwsSpec&");
    player_ = new VideoPlayer();
    preview_player_ = new VideoPlayer();

    //加载曾经播放过的文件
    //loadFile();

    listIndex = 0;
    setFocusPolicy(Qt::StrongFocus); //获取键盘监听
    ui->speedLabel->setVisible(false);
    ui->infoLabel->hide();

    connect(&timer,&QTimer::timeout,this,&MainWindow::onLongPressTimeout);   //右方向键长按倍速播放
    connect(player_,&VideoPlayer::stateChanged,this,&MainWindow::onPlayerStateChanged); //播放器状态转换
    connect(player_,&VideoPlayer::timeChanged,this,&MainWindow::onPlayerTimeChanged); //更新当前播放时间
    connect(player_,&VideoPlayer::initFinished,this,&MainWindow::onPlayerInitFinished); //初始化播放器参数
    connect(player_,&VideoPlayer::playFailed,this,&MainWindow::onPlayerPlayFailed); //播放出错；
    connect(player_,&VideoPlayer::frameDecoded,ui->videoWidget,&VideoWidget::onPlayerFrameDecoded);//视频显示
    connect(preview_player_,&VideoPlayer::frameDecoded,preview_,&VideoWidget::onPlayerFrameDecoded);    //预览图片显示
    connect(player_,&VideoPlayer::stateChanged,ui->videoWidget,&VideoWidget::onPlayerStateChanged);//预览线程中VideoWidget播放器状态转换
    connect(ui->progressSlider, &VideoSlider::clicked,this, &MainWindow::onSliderClicked);  //进度条点击
    connect(ui->progressSlider,&VideoSlider::preview,this,&MainWindow::onSliderMouseFoucs);    //进度条鼠标悬停（显示预览图片
    connect(ui->progressSlider,&VideoSlider::mouseleave,this,&MainWindow::onMouseLeaveSlider);    //进度条鼠标移动（关闭预览图片）
    connect(settingdialog, &settingDialog::startHardWareAccep, this, &MainWindow::on_setHardWare);          //设置硬件加速
    connect(settingdialog, &settingDialog::startSoftWareAccep, this, &MainWindow::on_setHardWare);          //设置软件加速
    connect(settingdialog,&settingDialog::updateSaveFilePath,this,&MainWindow::on_update_file_path);        //更新文件保存路径
    connect(settingdialog,&settingDialog::updateVideoSizeMode,ui->videoWidget,&VideoWidget::handleSizeModeChanged); //切换画面尺寸
    connect(settingdialog,&settingDialog::updateUserDecoder,this,&MainWindow::on_change_userDecoder);       //切换用户指定解码器
    connect(player_,&VideoPlayer::showMessage,this,&MainWindow::on_show_message_info);                      //显示提示消息
    connect(player_, &VideoPlayer::snapshotReady, this, [this](AVFrame *frame, const VideoPlayer::VideoSwsSpec &spec, const QString &path) {
        QFuture<bool> future = QtConcurrent::run([this,frame,spec,path]() {                                 //截图
            int imageSize = spec.size;
            QImage image(spec.width, spec.height, QImage::Format_RGB888);
            memcpy(image.bits(), frame->data[0], imageSize);
            bool ok = image.save(path);
            QString info = ok ? "截图保存成功：" + path: "截图失败！" + path;
            QMetaObject::invokeMethod(this, [this, info]() {
                messageInfo(info, 2000);
            }, Qt::QueuedConnection);
            return ok;
        });
    });

    //音量设置
    ui->volumeSlider->setRange(VideoPlayer::Volume::Min,VideoPlayer::Volume::Max);
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
    preview_player_->stopwithSignal();
    //释放资源
    delete ui;
    delete player_;
    delete preview_player_;
    delete picture_;

    player_ = nullptr;
    preview_player_ = nullptr;
    picture_ = nullptr;
}

void MainWindow::onPlayerStateChanged(VideoPlayer *player)
{
    VideoPlayer::State state = player->getState();
    if(VideoPlayer::Playing == state){
        ui->startBtn->setIcon(QIcon(":/SmartPlayer-icon/pause.png"));
        ui->progressSlider->setEnabled(true);
    }else{
        ui->startBtn->setIcon(QIcon(":/SmartPlayer-icon/start.png"));
    }
    if(state == VideoPlayer::Stopped){
        // ui->startBtn->setEnabled(false);
        ui->preVideoBtn->setEnabled(false);
        ui->nextVideoBtn->setEnabled(false);
        ui->back3sBtn->setEnabled(false);
        ui->forward3sBtn->setEnabled(false);
        ui->stopBtn->setEnabled(false);
        ui->progressSlider->setEnabled(false);
        ui->volumeBtn->setEnabled(false);
        ui->volumeSlider->setEnabled(false);
        ui->mutipleSPeed->setEnabled(false);
        ui->logoBtn->setVisible(true);
        ui->speedLabel->setVisible(false);
        ui->allTimeLabel->setText(getTimeText(0));
        ui->progressSlider->setValue(0);

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
    if(state == VideoPlayer::Paused){
        ui->logoBtn->setVisible(true);
    }
}

void MainWindow::onPlayerTimeChanged(VideoPlayer *player)
{
    ui->progressSlider->setValue(player->getTime());
}

void MainWindow::onPlayerInitFinished(VideoPlayer *player)
{
    int duration = player->getDuration();
    ui->progressSlider->setRange(0,duration);
    ui->allTimeLabel->setText(getTimeText(duration));
}

void MainWindow::onPlayerPlayFailed(VideoPlayer *player)
{
    QMessageBox::critical(nullptr, "提示", "哦豁，出现神秘的错误！");
}

void MainWindow::onSliderClicked(VideoSlider *slider)
{
    player_->setTime(slider->value());
}

void MainWindow::onSliderMouseFoucs(int seektime,int x)
{
    if (!preview_) return;

    preview_player_->setTime(seektime);
    preview_player_->updateSignal();

    // 安全获取 controlBar 在 videoWidget 的坐标
    QPoint globalBarPos = ui->controlBarContainer->mapToGlobal(QPoint(0, 0));
    QPoint barPosInVideoWidget = ui->videoWidget->mapFromGlobal(globalBarPos);

    // 将 slider 上的鼠标位置转换到 videoWidget 中
    QPoint globalMousePos = ui->progressSlider->mapToGlobal(QPoint(x, 0));
    QPoint videoMousePos = ui->videoWidget->mapFromGlobal(globalMousePos);

    // 设置预览图的坐标
    int xPos = videoMousePos.x() - preview_->width() / 2;
    int yPos = barPosInVideoWidget.y() - preview_->height() - 8;

    // 保证预览不越界
    xPos = qMax(0, qMin(xPos, ui->videoWidget->width() - preview_->width()));
    yPos = qMax(0, yPos);

    preview_->move(xPos, yPos);
    preview_->show();
}

void MainWindow::onMouseLeaveSlider(){
    preview_->hide();
}

void MainWindow::on_openFileBtn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "选择多媒体文件",
                                                    "/home",
                                                    "多媒体文件(*.mp4 *.avi *.mkv *.mp3 *.aac *.mov *.ts)");
    if(filePath == nullptr) return;
    addToFileList(filePath);
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
    VideoPlayer::State state = player_->getState();
    if(state == VideoPlayer::Playing){
        player_->pause();
    }else if(state == VideoPlayer::Paused){
        player_->play();
    }else if(state == VideoPlayer::Stopped && !fileList.empty()){
        qDebug() << "fileList[listIndex] = " << fileList[listIndex];
        player_->setFilename(fileList[listIndex]);
        player_->play();
    }
}


void MainWindow::on_stopBtn_clicked()
{
    player_->stop();
    preview_player_->stopwithSignal();
}


void MainWindow::on_preVideoBtn_clicked()
{
    if(fileList.count() == 0){
        QMessageBox::information(NULL,"当前列表为空","当前列表为空，无法切换上一个视频！",QMessageBox::Yes);
        return;
    }
    //如何正在播放视频，则清除帧，重新打开视频
    VideoPlayer::State state = player_->getState();
    if(state == VideoPlayer::Playing){
        player_->stop();
        preview_player_->stopwithSignal();
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
    //如何正在播放视频，则清除帧，重新打开视频
    VideoPlayer::State state = player_->getState();
    if(state == VideoPlayer::Playing){
        player_->stop();
        preview_player_->stopwithSignal();
    }

    listIndex += 1;
    //如果列表中就当前一个视频
    if(listIndex == fileList.count()){
        listIndex = 0;
    }
    ui->fileList->setCurrentRow(listIndex);
    play(fileList[listIndex]);
}


void MainWindow::on_back3sBtn_clicked()
{
    VideoPlayer::State state = player_->getState();
    if(state != VideoPlayer::Stopped){
        ui->progressSlider->changeValue(-10);
    }
}


void MainWindow::on_forward3sBtn_clicked()
{
    VideoPlayer::State state = player_->getState();
    if(state != VideoPlayer::Stopped){
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
    if(!this->isFullScreen()){
        this->showFullScreen();
        ui->videoWidget->setFullscreenMode(true);
        ui->fullScreenBtn->setIcon(QIcon(":/SmartPlayer-icon/exit_full_screen.png"));
    }else{
        this->showNormal();
        ui->videoWidget->setFullscreenMode(false);
        restoreControlBarParent();
        ui->fullScreenBtn->setIcon(QIcon(":/SmartPlayer-icon/full_screen.png"));
    }
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

            VideoItemWidget *itemWidget = new VideoItemWidget(picture_->getPreViewImage(fileInfo.at(i).absoluteFilePath(),110,65), fileInfo.at(i).fileName(), getTimeText(picture_->getDuration()));
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
    delete dir;
}


void MainWindow::on_clearListBtn_clicked()
{
    ui->fileList->clear();
    fileList.clear();
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

QString MainWindow::getTimeText(int value)
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
        if(!this->isFullScreen()){
            this->showFullScreen();
            ui->videoWidget->setFullscreenMode(true);
            ui->fullScreenBtn->setIcon(QIcon(":/SmartPlayer-icon/exit_full_screen.png"));
        }else{
            this->showNormal();
            ui->videoWidget->setFullscreenMode(false);
            restoreControlBarParent();
            ui->fullScreenBtn->setIcon(QIcon(":/SmartPlayer-icon/full_screen.png"));
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    VideoPlayer::State state = player_->getState();
    if(state != VideoPlayer::Stopped){
        //按空格键播放和暂停
        if(event->key() == Qt::Key_Space){
            state = player_->getState();
            if(state == VideoPlayer::Playing){
                player_->pause();
            }else{
                player_->play();
            }
        }

        //方向键右键，快进15秒
        else if(event->key() == Qt::Key_Right){
            state = player_->getState();
            if(state != VideoPlayer::Stopped){
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
            if(state != VideoPlayer::Stopped){
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
                this->showNormal();
                ui->videoWidget->setFullscreenMode(false);
                restoreControlBarParent();
                ui->fullScreenBtn->setIcon(QIcon(":/SmartPlayer-icon/full_screen.png"));
            }
        }
    }
}
void MainWindow::keyReleaseEvent(QKeyEvent* event){
    VideoPlayer::State state = player_->getState();
    if(state != VideoPlayer::Stopped){
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
        if(player_->getState() != VideoPlayer::Stopped){
            videoinfodialog->updateinformation(player_->getAVFormatContext(),player_->getFilename());
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
}

void MainWindow::closeFileList()
{
    ui->fileList->hide();
    ui->addFileBtn->hide();
    ui->addDirBtn->hide();
    ui->clearListBtn->hide();

    ui->videoWidget->resize(ui->playerPage->width(),ui->playerPage->height());
    ui->openListBtn->setIcon(QIcon(":/SmartPlayer-icon/left_arrow.png"));
}

void MainWindow::openVideoFromCommand(const QString &filePath)
{
    if(!filePath.isEmpty()){
        addToFileList(filePath);
        play(filePath);
    }
}

void MainWindow::addToFileList(QString filePath)
{
    if(!fileList.contains(filePath)){  //判断视频列表是否有当前视频文件
        QFileInfo temp(filePath);
        qDebug() << picture_->getDuration();
        QImage image = picture_->getPreViewImage(filePath,110,65);
        VideoItemWidget *itemWidget = new VideoItemWidget(image, temp.fileName(), getTimeText(picture_->getDuration()));
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
    //设置当前播放文件的索引
    for(int i =0;i < fileList.size();i++){
        if(filePath == fileList[i]){
            listIndex = i;
        }
    }
    if(listIndex == 0){
        ui->fileList->setCurrentRow(listIndex);
    }
}

void MainWindow::play(QString filePath)
{
    if(filePath.endsWith("mp4") || filePath.startsWith("file://"))
    {
        player_->setFilename(filePath);
        player_->play();
        preview_player_->setFilename(filePath);
        preview_player_->play_preview();
    }else{
        player_->setFilename(filePath);
        player_->play();
    }

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
    VideoPlayer::State state = player_->getState();
    if(state == VideoPlayer::Playing){
        player_->stop();
        //preview
        preview_player_->stopwithSignal();
    }

    QString fileAbsolutePath = item->data(Qt::UserRole).toString();

    for(int i=0;i<fileList.count();i++)
    {
        if(fileAbsolutePath == fileList[i]){
            listIndex = i;
        }
    }
    play(fileList[listIndex]);
}



void MainWindow::on_rtspButton_clicked()
{
    QInputDialog dialog;
    dialog.setWindowTitle("请输入网络流地址");
    dialog.setLabelText("地址：");

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
    VideoPlayer::State state = player_->getState();
    if(state == VideoPlayer::Stopped) return;
    QString filePath = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
    player_->requestSnapshot(filePath);
}


void MainWindow::on_settingBtn_clicked()
{
    settingdialog->show();
}

void MainWindow::on_setHardWare(bool on)
{
    VideoPlayer::State state = player_->getState();
    if(state == VideoPlayer::Stopped){
        player_->setIsHardWare(on);
        return;
    }
    player_->stop();
    preview_player_->stopwithSignal();

    player_->setIsHardWare(on);

    play(fileList[listIndex]);

}

void MainWindow::on_update_file_path(QString path)
{
    player_->setRootFilePath(path);
}

void MainWindow::on_change_userDecoder(QString decoder)
{
    if(decoder.isEmpty()) return;
    VideoPlayer::State state = player_->getState();
    if(state == VideoPlayer::Stopped){
        if(decoder == "默认(推荐)"){
            player_->setDecodeType(NULL);
        }else{
            player_->setDecodeType(decoder);
        }
        return;
    }else{
        player_->stop();
        preview_player_->stopwithSignal();

        if(decoder == "默认(推荐)"){
            player_->setDecodeType(NULL);
        }else{
            player_->setDecodeType(decoder);
        }

        play(fileList[listIndex]);
    }

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
        QWidget *currWidget = ui->fileList->itemWidget(current);
        if (auto videoItem = qobject_cast<VideoItemWidget *>(currWidget)) {
            videoItem->setFileNameTextColor(QColor(232, 88, 158)); // 设置为选中颜色
        }
    }
}

