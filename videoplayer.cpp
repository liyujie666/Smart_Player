#include "videoplayer.h"
#include <thread>
#include <QDebug>

#define AUDIO_MAX_PKT_SIZE 1000
#define VIDEO_MAX_PKT_SIZE 500
#define PREBUFFER_VIDEO_PACKETS 25 // 视频预缓冲包数
#define PREBUFFER_AUDIO_PACKETS 35 // 音频预缓冲包数（音频通常更密集）
static enum AVPixelFormat hwPixFmt_;

VideoPlayer::VideoPlayer(QObject *parent)
{
    //初始化SDL Audio
    if(SDL_Init(SDL_INIT_AUDIO)){
        qDebug() << "SDL_Init error" << SDL_GetError();
        emit playFailed(this);
        return;
    }
}

VideoPlayer::~VideoPlayer() {
    disconnect();
    stop();
    SDL_Quit();
}


void VideoPlayer::play()
{
    if(state_ == Playing) return;
    if(state_ == Stopped)
    {
        std::thread([this](){
            readFile();
        }).detach();
    }else{
        setState(Playing);
    }

}

void VideoPlayer::play_preview()
{
    if(state_ == Playing) return;
    if(state_ == Stopped)
    {
        std::thread([this](){
            startPreview();
        }).detach();
    }else{
        setState(Playing);
    }
}

void VideoPlayer::pause()
{
    if(state_ != Playing) return;
    setState(Paused);
}

void VideoPlayer::stop()
{
    if(state_ == Stopped) return;
    state_ = Stopped;
    //释放资源
    emit stateChanged(this);
    free();


}

void VideoPlayer::stopwithSignal()
{
    if(state_ == Stopped) return;
    state_ = Stopped;
    updateSignal();
    //释放资源
    emit stateChanged(this);
    free();
}

bool VideoPlayer::isPlaying()
{
    return state_ == Playing;
}

void VideoPlayer::setState(State state)
{
    if(state_ == state) return;
    state_ = state;
    emit stateChanged(this);
}

VideoPlayer::State VideoPlayer::getState()
{
    return state_;
}

void VideoPlayer::setFilename(QString &filename)
{
    if(filename.toLower().endsWith(".mp4") || filename.toLower().endsWith(".mkv"))
    {
        fileType_ = FILE;
    }else if(filename.startsWith("rtsp")){
        fileType_ = RTSP;
    }else if(filename.startsWith("rtmp")){
        fileType_ = RTMP;
    }
    std::string temp = filename.toStdString();
    const char *name = temp.c_str();
    memcpy(filename_,name,strlen(name)+1);

}

int VideoPlayer::getDuration()
{
    if(fileType_ == FILE){
        return fmtCtx_ ? round(fmtCtx_->duration * av_q2d(AV_TIME_BASE_Q)) : 0; //单位转化成秒
    }else{
        return 0;
    }

}

int VideoPlayer::getTime()
{
    if(hasAudio_){
        return round(aTime_);
    }else {
        return round(vTime_);
    }

}

void VideoPlayer::setTime(int seekTime)
{
    seekTime_ = seekTime;
}

void VideoPlayer::setVolume(int volume)
{
    volume_ = volume;
}

int VideoPlayer::getVolume()
{
    return volume_;
}

void VideoPlayer::setMute(bool mute)
{
    mute_ = mute;
}

bool VideoPlayer::isMute()
{
    return mute_;
}

void VideoPlayer::updateSignal()
{
    previewMutex_.lock();
    previewMutex_.signal();
    previewMutex_.unlock();
}

void VideoPlayer::setSpeed(int index)
{
    currentSpeedIndex = index;
}

AVFormatContext *VideoPlayer::getAVFormatContext()
{
    return fmtCtx_;
}

char *VideoPlayer::getFilename()
{
    return filename_;
}

VideoPlayer::Type VideoPlayer::getFileType()
{
    return fileType_;
}

int VideoPlayer::open(const char *fileName)
{
    avdevice_register_all();
    avformat_network_init();

    AVDictionary* opts = nullptr;
    av_dict_set(&opts,"rtsp_transport","tcp",0);
    av_dict_set(&opts,"rtmp_transport","tcp", 0); // 强制使用TCP
    av_dict_set(&opts, "buffer_size", "1024000", 0);
    av_dict_set(&opts,"timeout","2000000",0);
    av_dict_set(&opts, "reconnect", "1", 0);        // 启用自动重连

    qDebug() << fileName;
    if(avformat_open_input(&fmtCtx_,fileName,nullptr,&opts) < 0)
    {
        qDebug() << "avformat_open_input failed!";
        return -1;
    }

    if(opts) av_dict_free(&opts);

    if(avformat_find_stream_info(fmtCtx_,nullptr) < 0)
    {
        qDebug() << "avformat_find_stream_info failed!";
        return -1;
    }

    //打印流信息
    av_dump_format(fmtCtx_,0,filename_,0);
    fflush(stderr);

    return 0;
}



void VideoPlayer::readFile()
{
    int ret = 0;
    //打开文件，创建解封装上下文

    ret = open(filename_);
    END(open);

    //初始化音频信息
    hasAudio_ = initAudioInfo() >= 0;

    //初始化视频信息
    hasVideo_ = initVideoInfo() >= 0;

    if(!hasAudio_ && !hasVideo_){
        fataError();
        return;
    }

    if(hasAudio_){
        //初始化倍数过滤器
        initFilter(&graph_1, &srcFilterCtx_1, &sinkFilterCtx_1, "0.5");
        initFilter(&graph_2, &srcFilterCtx_2, &sinkFilterCtx_2, "1.0");
        initFilter(&graph_3, &srcFilterCtx_3, &sinkFilterCtx_3, "1.5");
        initFilter(&graph_4, &srcFilterCtx_4, &sinkFilterCtx_4, "2.0");
    }

    emit initFinished(this);

    setState(Playing);

    //解码音频，开启音频播放

    if(hasAudio_){
        //解码音频，开启音频播放
        SDL_PauseAudio(0);

        //解码视频
        std::thread([this](){
            decodeVideo(false);
        }).detach();

        //从视频文件中读物数据
        AVPacket pkt,keypkt,temp;
        while(state_ != Stopped){
            // 处理seek操作
            if (seekTime_ >= 0) {
                int streamIdx;
                //选择使用流作为时间基准 seek

                if (hasAudio_) {
                    streamIdx = aStream_->index;
                }else{
                    streamIdx = vStream_->index;
                }

                //定位到seekTime前10秒，跳转到足够早的位置，确保能扫描到所有潜在的关键帧。
                AVRational timeBase = fmtCtx_->streams[streamIdx]->time_base;
                int64_t ts;
                ts = (seekTime_ < 10 ? 0 : seekTime_ - 15) / av_q2d(timeBase);

                ret = av_seek_frame(fmtCtx_, streamIdx, ts, AVSEEK_FLAG_BACKWARD);
                if (ret < 0) { // seek失败
                    qDebug() << "seek失败" << seekTime_ << ts << streamIdx;
                    seekTime_ = -1;
                } else {

                    // 清空数据包
                    clearAudioPktList();
                    clearVideoPktList();
                    double Time = 0;

                    // 获取最近关键帧（从跳转后的位置开始，逐个读取数据包，找到第一个时间戳 超过 seekTime 的关键帧）
                    av_read_frame(fmtCtx_, &temp);
                    while(1){
                        ret = av_read_frame(fmtCtx_, &keypkt);
                        if (ret == 0) {
                            if (keypkt.stream_index == vStream_->index) {
                                if(vPktList_.empty()){
                                    //只需要关键帧
                                    if(!(keypkt.flags &AV_PKT_FLAG_KEY)){
                                        av_packet_unref(&keypkt);
                                        continue;
                                    }

                                    Time = av_q2d(vStream_->time_base) * keypkt.dts;
                                    if(Time > seekTime_){
                                        av_packet_unref(&keypkt);
                                        //最近关键帧添加到视频包队列
                                        addVideoPkt(&temp);
                                        av_packet_unref(&temp);
                                        break;
                                    }
                                    av_packet_unref(&temp);
                                    av_packet_ref(&temp,&keypkt);
                                    av_packet_unref(&keypkt);

                                }
                            }
                        }
                    }
                    //关键帧的时间
                    ts = Time / av_q2d(timeBase);
                    //重新定位到关键帧
                    ret = av_seek_frame(fmtCtx_, streamIdx, ts, AVSEEK_FLAG_BACKWARD);
                    if (ret < 0) { // seek失败
                        qDebug() << "seek失败" << Time << ts << streamIdx;
                        seekTime_ = -1;
                    } else {

                        //将最近关键帧到seekTime里的视频包读入队列
                        while(1){
                            ret = av_read_frame(fmtCtx_, &keypkt);
                            if (ret == 0) {
                                if (keypkt.stream_index == vStream_->index) {
                                    Time = av_q2d(vStream_->time_base) * keypkt.dts;
                                    if(Time > seekTime_){
                                        av_packet_unref(&keypkt);
                                        break;
                                    }
                                    addVideoPkt(&keypkt);
                                    av_packet_unref(&keypkt);
                                }
                            }
                        }

                        //定位到seekTime
                        ts = seekTime_ / av_q2d(timeBase);
                        ret = av_seek_frame(fmtCtx_, streamIdx, ts, AVSEEK_FLAG_BACKWARD);
                        if (ret < 0) { // seek失败
                            qDebug() << "seek失败" << seekTime_ << ts << streamIdx;
                            seekTime_ = -1;
                        } else {
                            qDebug() << "seek成功" << Time << ts << streamIdx;
                            clearAudioPktList();
                            vSeekTime_ = seekTime_;
                            aSeekTime_ = seekTime_;
                            seekTime_ = -1;
                            // 恢复时钟
                            aTime_ = 0;
                            vTime_ = 0;

                        }
                    }

                }
            }

            if(!isReady){
                // 先缓存一些音视频包，防止片头出现卡顿
                while (
                    (vPktList_.size() < PREBUFFER_VIDEO_PACKETS ||
                     aPktList_.size() < PREBUFFER_AUDIO_PACKETS) &&
                    av_read_frame(fmtCtx_, &pkt) == 0
                    ) {
                    pkt.time_base = vStream_->time_base;
                    if (pkt.stream_index == vStream_->index) {
                        // 视频包：仅预存关键帧（I帧）
                        if (vPktList_.size() < PREBUFFER_VIDEO_PACKETS) {
                            addVideoPkt(&pkt);
                            av_packet_unref(&pkt);
                        }
                    } else if (pkt.stream_index == aStream_->index) {
                        // 音频包：直接预存
                        if (aPktList_.size() < PREBUFFER_AUDIO_PACKETS) {
                            addAudioPkt(&pkt);
                            av_packet_unref(&pkt);
                        }
                    } else {
                        av_packet_unref(&pkt);
                    }
                }

                if (vPktList_.size() >= PREBUFFER_VIDEO_PACKETS && aPktList_.size() >= PREBUFFER_AUDIO_PACKETS) {
                    qDebug() << "预缓冲完成，开始播放";
                    isReady.store(true);
                }
            }
            int vSize = vPktList_.size();
            int aSize = aPktList_.size();

            if(vSize >= VIDEO_MAX_PKT_SIZE || aSize >= AUDIO_MAX_PKT_SIZE){
                continue;
            }

            ret = av_read_frame(fmtCtx_, &pkt);
            if(ret == 0){

                pkt.time_base = vStream_->time_base;
                if(pkt.stream_index == aStream_->index){
                    addAudioPkt(&pkt);
                }else if(pkt.stream_index == vStream_->index){
                    if(vPktList_.empty()){
                        if(!(pkt.flags & AV_PKT_FLAG_KEY)){
                            av_packet_unref(&pkt);
                            continue;
                        }
                    }
                    addVideoPkt(&pkt);
                    av_packet_unref(&pkt);
                }else{ // 非视频，音频包
                    av_packet_unref(&pkt);
                }
            }else if(ret == AVERROR_EOF){ // 读到了文件的尾部

                if(vSize == 0 || aSize == 0){
                    // 等到播放完毕，包队列取完
                    fmtCtxCanFree_ = true;
                    qDebug() << "队列已空";
                    break;
                }
            }else{
                ERROR_BUF;
                qDebug() << "av_read_frame fialed:" << errbuf;
                continue;
            }
        }
    }else{
        std::thread([this](){
            decodeVideoNoAudio();
        }).detach();

        AVPacket pkt;
        while(state_ != Stopped){

            if(!isReady){
                // 先缓存一些音视频包，防止片头出现卡顿
                while (vPktList_.size() < PREBUFFER_VIDEO_PACKETS && av_read_frame(fmtCtx_, &pkt) == 0) {
                    pkt.time_base = vStream_->time_base;
                    if (pkt.stream_index == vStream_->index) {
                        // 视频包：仅预存关键帧（I帧）
                        if (vPktList_.size() < PREBUFFER_VIDEO_PACKETS) {
                            addVideoPkt(&pkt);
                            av_packet_unref(&pkt);
                        }
                    }else {
                        av_packet_unref(&pkt);
                    }
                }
                if (vPktList_.size() >= PREBUFFER_VIDEO_PACKETS) {
                    qDebug() << "预缓冲完成，开始播放";
                    isReady.store(true);
                }
            }
            int vSize = vPktList_.size();
            if(vSize >= VIDEO_MAX_PKT_SIZE){
                continue;
            }

            ret = av_read_frame(fmtCtx_, &pkt);
            if(ret == 0){
                pkt.time_base = vStream_->time_base;
                if(pkt.stream_index == vStream_->index)
                {
                    if(vPktList_.empty()){
                        if(!(pkt.flags & AV_PKT_FLAG_KEY)){
                            av_packet_unref(&pkt);
                            continue;
                        }
                    }
                    addVideoPkt(&pkt);
                    av_packet_unref(&pkt);
                }
                else
                { // 非视频，音频包
                    av_packet_unref(&pkt);
                }
            }else if(ret == AVERROR_EOF){ // 读到了文件的尾部
                if(vSize == 0){
                    // 等到播放完毕，包队列取完
                    fmtCtxCanFree_ = true;
                    qDebug() << "队列已空";
                    break;
                }
            }else{
                ERROR_BUF;
                qDebug() << "av_read_frame fialed:" << errbuf;
                continue;
            }
        }
    }

    if(fmtCtxCanFree_){
        stop();
    }else{
        fmtCtxCanFree_ = true;
    }

}

void VideoPlayer::startPreview()
{
    int ret = 0;
    //创建解封装上下文、打开文件
    ret = avformat_open_input(&fmtCtx_,filename_,nullptr,nullptr);
    END(avformat_open_input);

    //检索流信息
    ret = avformat_find_stream_info(fmtCtx_,nullptr);
    END(avformat_find_stream_info);

    //抛弃音频
    hasAudio_ = false;

    //初始化视频信息
    hasVideo_ = initVideoInfo() >= 0;
    if(!hasVideo_ && !hasAudio_){
        fataError();
        return;
    }

    emit initFinished(this);
    setState(Playing);

    std::thread([this](){
        decodeVideo(true);
    }).detach();

    qDebug() <<"preview start";
    AVPacket keypkt,temp;
    while (state_ != Stopped) {
        previewMutex_.lock();
        previewMutex_.wait();
        previewMutex_.unlock();

        if (seekTime_ >= 0) {
            int streamIdx;
            //选择使用流作为时间基准 seek
            if (hasVideo_) {
                streamIdx = vStream_->index;
            }else{
                return;
            }

            //定位到seekTime前20秒
            AVRational timeBase = fmtCtx_->streams[streamIdx]->time_base;
            int64_t ts;
            ts = (seekTime_ < 2 ? 0 : seekTime_ - 2) / av_q2d(timeBase);

            ret = av_seek_frame(fmtCtx_, streamIdx, ts, AVSEEK_FLAG_BACKWARD);
            if (ret < 0) { // seek失败
                qDebug() << "seek失败" << seekTime_ << ts << streamIdx;
                seekTime_ = -1;
            } else {

                // 清空数据包
                clearVideoPktList();
                double Time = 0;

                // 获取最近关键帧
                av_read_frame(fmtCtx_, &temp);
                while(1){
                    ret = av_read_frame(fmtCtx_, &keypkt);
                    if (ret == 0) {
                        if (keypkt.stream_index == vStream_->index) {
                            if(vPktList_.empty()){
                                if(!(keypkt.flags &AV_PKT_FLAG_KEY)){
                                    continue;
                                }

                                Time = av_q2d(vStream_->time_base) * keypkt.dts;
                                if(Time > seekTime_){
                                    av_packet_unref(&keypkt);
                                    addVideoPkt(&temp);
                                    break;
                                }
                                av_packet_unref(&temp);
                                av_packet_ref(&temp,&keypkt);
                                av_packet_unref(&keypkt);

                            }
                        }
                    }
                }
                ts = Time / av_q2d(timeBase);
                ret = av_seek_frame(fmtCtx_, streamIdx, ts, AVSEEK_FLAG_BACKWARD);
                if (ret < 0) { // seek失败
                    qDebug() << "seek失败" << Time << ts << streamIdx;
                    seekTime_ = -1;
                } else {

                    clearVideoPktList();
                    //将最近关键帧到seekTime里的视频包读入队列
                    while(1){
                        ret = av_read_frame(fmtCtx_, &keypkt);
                        if (ret == 0) {
                            if (keypkt.stream_index == vStream_->index) {
                                Time = av_q2d(vStream_->time_base) * keypkt.dts;
                                if(Time > seekTime_){
                                    addVideoPkt(&keypkt);
                                    break;
                                }
                                addVideoPkt(&keypkt);
                            }
                        }
                    }
                    vSeekTime_ = seekTime_;
                    seekTime_ = -1;
                    // 恢复时钟
                    vTime_ = 0;

                }

            }
        }
    }
    if (fmtCtxCanFree_) {
        stop();
    }else{
        fmtCtxCanFree_ = true;
    }
}

static enum AVPixelFormat get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
    const enum AVPixelFormat* p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hwPixFmt_)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}


int VideoPlayer::initDecoder(AVCodecContext **decodecCtx, AVStream **stream, AVMediaType type)
{
    const AVCodec* decoder = nullptr;

    //查找指定流
    int ret = av_find_best_stream(fmtCtx_, type, -1, -1, &decoder, 0);
    if(ret < 0){
        qDebug() << "av_find_best_stream failed ,type = " << type;
        return -1;
    }
    // RET(av_find_best_stream);

    int streamIndex = ret;
    *stream = fmtCtx_->streams[streamIndex];
    if (!*stream) {
        qDebug() << "stream is empty!";
        return -1;
    }

    QString info;
    //处理用户指定的解码器
    if(!decodeType_.isEmpty() && type == AVMEDIA_TYPE_VIDEO){

        const AVCodec* userDecoder = avcodec_find_decoder_by_name(decodeType_.toStdString().c_str());

        if(userDecoder){

            //检查解码器是否支持当前stream的codec_id
            if(userDecoder->id != (*stream)->codecpar->codec_id){
                info = "当前解码器与视频编码格式不匹配！已使用默认解码器！";

                decoder = avcodec_find_decoder((*stream)->codecpar->codec_id);
            }else{
                decoder = userDecoder;
                info = "解码器切换成功！";
            }
        }else{
            info = "未找到当前解码器" + QString(decodeType_);
        }

        emit showMessage(info,2000);
    }

    // 查找支持的硬件像素格式
    if(useHwAccel_ && type == AVMEDIA_TYPE_VIDEO){
        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
            if (!config) {
                qDebug() << "Decoder does not support CUDA.";
                break;
            }
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                config->device_type == hwType_) {
                hwPixFmt_ = config->pix_fmt;
                hw_pix_fmt_ = hwPixFmt_;
                break;
            }
        }
    }

    // 分配上下文
    *decodecCtx = avcodec_alloc_context3(decoder);
    if (!*decodecCtx) {
        qDebug() << "avcodec_alloc_context3 failed!";
        return -1;
    }

    // 复制参数到解码器上下文
    ret = avcodec_parameters_to_context(*decodecCtx, (*stream)->codecpar);
    RET(avcodec_parameters_to_context);


    // 开启硬件加速
    if (useHwAccel_) {

        // 创建硬件设备上下文
        ret = av_hwdevice_ctx_create(&hwDeviceCtx_, hwType_, nullptr, nullptr, 0);
        if (ret < 0) {
            qDebug() << "Failed to create hw device ctx.";
            return -1;
        }
        (*decodecCtx)->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
        (*decodecCtx)->get_format = get_hw_format;  // 注册格式选择函数
        emit showMessage("硬件加速成功！",2000);
    }

    // 打开解码器（会触发 get_format 回调）
    ret = avcodec_open2(*decodecCtx, decoder, nullptr);
    RET(avcodec_open2);

    return 0;

}

void VideoPlayer::setRootFilePath(const QString &path)
{
    rootFilePath_ = path;
}


void VideoPlayer::requestSnapshot(const QString &filename)
{
    snapshotPath_ = rootFilePath_+ "/" + filename;
    takeSnapshot_ = true;
}

void VideoPlayer::setIsHardWare(bool on)
{
    useHwAccel_ = on;
}

void VideoPlayer::free()
{
    while(hasAudio_ && !aCanFree_);
    while(hasVideo_ && !vCanFree_);
    while(!fmtCtxCanFree_);
    if(hasAudio_){
        freeAudio();
        freeFilter();
    }
    freeVideo();
    if(fmtCtx_){
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
    }
    fmtCtxCanFree_ = false;
    seekTime_ = -1;
}

void VideoPlayer::fataError()
{
    state_ = Playing;
    stop();
    emit playFailed(this);
}

void VideoPlayer::setDecodeType(const QString &newDecodeType)
{
    decodeType_ = newDecodeType;
}


int VideoPlayer::initFilter(AVFilterGraph **graph, AVFilterContext **srcFilterCtx, AVFilterContext **sinkFilterCtx, char *value) {
    //注册过滤器
    //avfilter_register_all();
    *graph = avfilter_graph_alloc();

    //源过滤器和格式转换过滤器参数
    std::string s1="sample_rate="+std::to_string(aCodecCtx_->sample_rate)+":sample_fmt="+av_get_sample_fmt_name(aCodecCtx_->sample_fmt)+":channel_layout="+std::to_string(aCodecCtx_->ch_layout.u.mask);
    std::string s2="sample_rates="+std::to_string(aCodecCtx_->sample_rate)+":sample_fmts="+av_get_sample_fmt_name(aCodecCtx_->sample_fmt)+":channel_layouts="+std::to_string(aCodecCtx_->ch_layout.u.mask);

    //创建源过滤器
    const AVFilter *srcFilter=avfilter_get_by_name("abuffer");
    *srcFilterCtx=avfilter_graph_alloc_filter(*graph,srcFilter,"src");
    if (avfilter_init_str(*srcFilterCtx, s1.c_str()) < 0) {
        qDebug()<<"初始化源过滤器失败";
        return -1;
    }


    //创建变速过滤器
    const AVFilter *atempoFilter = avfilter_get_by_name("atempo");
    AVFilterContext *atempoFilterCtx = avfilter_graph_alloc_filter(*graph, atempoFilter, "atempo");
    AVDictionary *args = NULL;
    av_dict_set(&args, "tempo", value, 0);//根据value的值调节速度
    if (avfilter_init_dict(atempoFilterCtx, &args) < 0) {
        qDebug()<<"初始化变速过滤器失败";
        return -1;
    }


    //创建格式转化过滤器
    const AVFilter *aformatFilter = avfilter_get_by_name("aformat");
    AVFilterContext *aformatFilterCtx = avfilter_graph_alloc_filter(*graph, aformatFilter, "aformat");
    if (avfilter_init_str(aformatFilterCtx, s2.c_str()) < 0) {
        qDebug()<<"初始化格式转化过滤器失败";
        return -1;
    }


    //创建接收过滤器
    const AVFilter *sinkFilter=avfilter_get_by_name("abuffersink");
    *sinkFilterCtx=avfilter_graph_alloc_filter(*graph,sinkFilter,"sink");
    if (avfilter_init_dict(*sinkFilterCtx, NULL) < 0) {
        qDebug()<<"初始化变速过滤器失败";
        return -1;
    }

    //链接过滤器
    if(avfilter_link(*srcFilterCtx,0,atempoFilterCtx,0) != 0){
        qDebug()<<"没link成功变速过滤器";
        return -1;
    }
    if(avfilter_link(atempoFilterCtx,0,aformatFilterCtx,0) != 0){
        qDebug()<<"没link成功格式转化过滤器";
        return -1;
    }
    if(avfilter_link(aformatFilterCtx,0,*sinkFilterCtx,0) != 0){
        qDebug()<<"没link成功接收过滤器";
        return -1;
    }


    //配置图
    if (avfilter_graph_config(*graph, NULL) < 0) {
        qDebug()<<"配置graph失败";
        return -1;
    }

    return 0;
}

void VideoPlayer::freeFilter() {

    if(srcFilterCtx_1 != nullptr){
        avfilter_free(srcFilterCtx_1);
        srcFilterCtx_1 = nullptr;
    }
    if(sinkFilterCtx_1 != nullptr){
        avfilter_free(sinkFilterCtx_1);
        sinkFilterCtx_1 = nullptr;
    }
    if(graph_1 != nullptr){
        avfilter_graph_free(&graph_1);
        graph_1 = nullptr;
    }
    if(srcFilterCtx_2 != nullptr){
        avfilter_free(srcFilterCtx_2);
        srcFilterCtx_2 = nullptr;
    }
    if(sinkFilterCtx_2 != nullptr){
        avfilter_free(sinkFilterCtx_2);
        sinkFilterCtx_2 = nullptr;
    }
    if(graph_2 != nullptr){
        avfilter_graph_free(&graph_2);
        graph_2 = nullptr;
    }
    if(srcFilterCtx_3 != nullptr){
        avfilter_free(srcFilterCtx_3);
        srcFilterCtx_3 = nullptr;
    }
    if(sinkFilterCtx_3 != nullptr){
        avfilter_free(sinkFilterCtx_3);
        sinkFilterCtx_3 = nullptr;
    }
    if(graph_3 != nullptr){
        avfilter_graph_free(&graph_3);
        graph_3 = nullptr;
    }
    if(srcFilterCtx_4 != nullptr){
        avfilter_free(srcFilterCtx_4);
        srcFilterCtx_4 = nullptr;
    }
    if(sinkFilterCtx_4 != nullptr){
        avfilter_free(sinkFilterCtx_4);
        sinkFilterCtx_4 = nullptr;
    }
    if(graph_4 != nullptr){
        avfilter_graph_free(&graph_4);
        graph_4 = nullptr;
    }

    qDebug() << "过滤器资源已被释放";
}
