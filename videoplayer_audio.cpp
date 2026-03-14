#include"videoplayer.h"
#include <QDebug>

int VideoPlayer::initAudioInfo(){
    int ret = 0;
    //初始化编码器
    ret = initDecoder(&aCodecCtx_,&aStream_,AVMEDIA_TYPE_AUDIO);
    RET(initDecoder);

    //初始化音频重采样
    ret = initSwr();
    RET(initSwr);

    //初始化SDL
    ret = initSDL();
    RET(initSDL);

    return 0;
}

int VideoPlayer::initSwr()
{
    int ret = 0;
    //重采样输入参数
    aSwrInSpec_.sampleFmt = aCodecCtx_->sample_fmt;
    aSwrInSpec_.sampleRate = aCodecCtx_->sample_rate;
    aSwrInSpec_.chLayout = aCodecCtx_->ch_layout;
    aSwrInSpec_.chs = aCodecCtx_->ch_layout.nb_channels;

    //重采样输出参数
    aSwrOutSpec_.sampleFmt = AV_SAMPLE_FMT_S16;
    aSwrOutSpec_.sampleRate = 44100;
    // aSwrOutSpec_.chLayout = AV_CHANNEL_LAYOUT_STEREO;
    aSwrOutSpec_.chLayout = aCodecCtx_->ch_layout;
    aSwrOutSpec_.chs = aCodecCtx_->ch_layout.nb_channels;
    aSwrOutSpec_.bytesPerSampleFmt = aSwrOutSpec_.chs * av_get_bytes_per_sample(aSwrOutSpec_.sampleFmt);

    //创建重采样上下文
    ret = swr_alloc_set_opts2(&aSwrCtx_,
                                   &aSwrOutSpec_.chLayout,aSwrOutSpec_.sampleFmt,aSwrOutSpec_.sampleRate,
                                   &aSwrInSpec_.chLayout,aSwrInSpec_.sampleFmt,aSwrInSpec_.sampleRate,
                                   0,nullptr);
    RET(swr_alloc_set_opts2);

    //初始化重采样上下文
    ret = swr_init(aSwrCtx_);
    RET(swr_init);

    // 初始化重采样的输入frame
    aSwrInFrame_ = av_frame_alloc();
    if(!aSwrInFrame_){
        qDebug() << "aSwrInFrame__alloc failed!";
        return -1;
    }

    // 初始化重采样的输出frame

    aSwrOutFrame_ = av_frame_alloc();
    if(!aSwrOutFrame_){
        qDebug() << "aSwrOutFrame__alloc failed!";
        return -1;
    }

    // 初始化输出frame data空间
    ret = av_samples_alloc(aSwrOutFrame_->data,
                           aSwrOutFrame_->linesize,
                           aSwrOutSpec_.chs,
                           4096,
                           aSwrOutSpec_.sampleFmt,
                           1);
    RET(av_samples_alloc);

    return 0;
}

int VideoPlayer::initSDL()
{
    SDL_AudioSpec spec;
    spec.freq = aSwrOutSpec_.sampleRate;
    spec.format = AUDIO_S16LSB;
    spec.samples = 512;//音频缓冲区的样本数量
    spec.channels = aSwrOutSpec_.chs;
    spec.callback = sdlAudioCallbackFunc;
    spec.userdata = this;

    //打开音频设备
    if(SDL_OpenAudio(&spec,nullptr)){
        qDebug() << "SDL_OpenAudio failed!" << SDL_GetError();
        return -1;
    }

    return 0;

}

void VideoPlayer::addAudioPkt(AVPacket *pkt)
{
    aMutex_.lock();
    AVPacket *temp_pkt = av_packet_alloc();
    av_packet_move_ref(temp_pkt,pkt);
    aPktList_.push_back(temp_pkt);
    aMutex_.signal();
    aMutex_.unlock();
}

void VideoPlayer::clearAudioPktList()
{
    aMutex_.lock();
    for(AVPacket *pkt : aPktList_){
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }
    aPktList_.clear();
    aMutex_.unlock();
}

void VideoPlayer::freeAudio() {
    clearAudioPktList();

    if (aCodecCtx_) {
        avcodec_flush_buffers(aCodecCtx_);
        avcodec_free_context(&aCodecCtx_);
        aCodecCtx_ = nullptr;
    }
    if (aSwrCtx_) {
        swr_free(&aSwrCtx_);
        aSwrCtx_ = nullptr;
    }
    if (aSwrInFrame_) {
        av_frame_free(&aSwrInFrame_);
        aSwrInFrame_ = nullptr;
    }
    if (aSwrOutFrame_) {
        av_frame_free(&aSwrOutFrame_);
        aSwrOutFrame_ = nullptr;
    }

    // 停止播放
    SDL_PauseAudio(1);
    SDL_CloseAudio();

    aTime_ = 0;
    aSwrOutIndex_ = 0;
    aSwrOutSize_ = 0;
    aStream_ = nullptr;
    aCanFree_ = false;
    aSeekTime_ = -1;
    hasAudio_ = false;

    qDebug() << "音频资源已被释放";
}

void VideoPlayer::sdlAudioCallbackFunc(void *userdata, Uint8 *stream, int len)
{
    VideoPlayer *player = (VideoPlayer*) userdata;
    player->sdlAudioCallback(stream,len);
}

void VideoPlayer::sdlAudioCallback(Uint8 *stream, int len)
{
    //清零
    SDL_memset(stream,0,len);

    while(len > 0)
    {
        if(state_ == Paused) break;
        if(state_ == Stopped){
            aCanFree_ = true;
            break;
        }

        /*
            解码pkt,获取新的PCM数据
        */
        if(aSwrOutIndex_ >= aSwrOutSize_){   //如果当前音频数据已经用完
            aSwrOutSize_ = decodeAudio();   //解码获取新的音频数据
            aSwrOutIndex_ = 0;
            //如果解码后没有音频数据，则播放静音数据
            if(aSwrOutSize_ <= 0){
                aSwrOutSize_ = 1024;
                memset(aSwrOutFrame_->data[0],0,aSwrOutSize_);
            }
        }

        //次需要填充到 stream 中的音频数据长度
        int fillLen = aSwrOutSize_ - aSwrOutIndex_;
        fillLen = std::min(fillLen,len);

        //获取当前的音量
        int volume = mute_ ? 0 : ((volume_ * 1.0 / Max) * SDL_MIX_MAXVOLUME);

        //填充SDL缓冲区
        SDL_MixAudio(stream,aSwrOutFrame_->data[0] + aSwrOutIndex_,fillLen,volume);

        //移动偏移量
        len -= fillLen;
        stream += fillLen;
        aSwrOutIndex_ += fillLen;
    }
}

int VideoPlayer::decodeAudio()
{
    aMutex_.lock();

    //判断音频包队列是否为空
    if(aPktList_.empty()){
        aMutex_.unlock();
        return 0;
    }

    //取出音频包
    AVPacket *pkt = aPktList_.front();
    aPktList_.pop_front();
    aMutex_.unlock();

    //保存音频时钟
    if(pkt->pts != AV_NOPTS_VALUE){  //音频时钟无效判断
        aTime_ = av_q2d(aStream_->time_base) * pkt->pts;  //获取包时间
        //通知外界 播放时间点发生了变化
        emit timeChanged(this);
    }

    if(aSeekTime_ >= 0){
        if(aTime_ < aSeekTime_){
            //释放pkt
            av_packet_unref(pkt);
            return 0;
        }else{
            aSeekTime_ = -1;
        }
    }

    // 发送压缩数据到解码器
    int ret = avcodec_send_packet(aCodecCtx_,pkt);
    av_packet_unref(pkt);
    RET(avcodec_send_packet);

    // 获取解码后的数据
    ret = avcodec_receive_frame(aCodecCtx_,aSwrInFrame_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 0;
    } else
        RET(avcodec_receive_frame);

    //解码后数据送入过滤器
    switch(currentSpeedIndex){
    case 1:
        av_buffersrc_add_frame(srcFilterCtx_1,aSwrInFrame_);
        av_buffersink_get_frame(sinkFilterCtx_1,aSwrInFrame_);
        break;
    case 2:
        av_buffersrc_add_frame(srcFilterCtx_2,aSwrInFrame_);
        av_buffersink_get_frame(sinkFilterCtx_2,aSwrInFrame_);
        break;
    case 3:
        av_buffersrc_add_frame(srcFilterCtx_3,aSwrInFrame_);
        av_buffersink_get_frame(sinkFilterCtx_3,aSwrInFrame_);
        break;
    case 4:
        av_buffersrc_add_frame(srcFilterCtx_4,aSwrInFrame_);
        av_buffersink_get_frame(sinkFilterCtx_4,aSwrInFrame_);
        break;
    default:
        av_buffersrc_add_frame(srcFilterCtx_2,aSwrInFrame_);
        av_buffersink_get_frame(sinkFilterCtx_2,aSwrInFrame_);
    };


    // 重采样输出的样本数
    //a*b/c 向上取整
    int outSamples = av_rescale_rnd(aSwrOutSpec_.sampleRate,aSwrInFrame_->nb_samples,aSwrInSpec_.sampleRate,AV_ROUND_UP);

    //重采样  return number of samples output per channel, negative value on error
    ret = swr_convert(aSwrCtx_,
                      aSwrOutFrame_->data,
                      outSamples,
                      (const uint8_t **)aSwrInFrame_->data,
                      aSwrInFrame_->nb_samples);

    RET(swr_convert);
    return ret * aSwrOutSpec_.bytesPerSampleFmt;
}
