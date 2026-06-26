#include "playercore.h"
#include "pool/gloabalpool.h"
#include "app/configmanager.h"
#include <QDebug>
#include <QImage>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDateTime>
#include <QtConcurrent>
#include <cstring>

PlayerCore::PlayerCore(QObject *parent)
    : QObject(parent), state_(Stopped), is_exit_(false), is_seek_(false)
{
    audio_pkt_queue_ = new AVPacketQueue();
    video_pkt_queue_ = new AVPacketQueue();
    audio_frame_queue_ = new AVFrameQueue();
    video_frame_queue_ = new AVFrameQueue();
    //subtitle_queue_ = new SubtitleQueue();
    sync_clock_ = new AVSyncClock();
    asr_manager_ = std::make_unique<AsrManager>();
}

PlayerCore::~PlayerCore()
{
    stop();

    delete audio_pkt_queue_; audio_pkt_queue_ = nullptr;
    delete video_pkt_queue_; video_pkt_queue_ = nullptr;
    delete audio_frame_queue_; audio_frame_queue_ = nullptr;
    delete video_frame_queue_; video_frame_queue_ = nullptr;

    if (sync_clock_) {
        sync_clock_->reset();
        delete sync_clock_;
        sync_clock_ = nullptr;
    }

}


void PlayerCore::open(const QString &url)
{
    if (!url.startsWith("rtsp://") && !url.startsWith("rtmp://") && !url.startsWith("http://") && !url.startsWith("https://"))
    {
        stop();
        bool ret = openInternal(url);
        emit openResult(ret);
        return;
    }

    QtConcurrent::run([this, url]() {
        stop();
        bool ret = openInternal(url);
        emit openResult(ret);
    });
}

bool PlayerCore::openInternal(const QString &url)
{

    int ret=0;

    // 解复用器
    demuxer_ = new Demuxer();
    ret = demuxer_->open(url.toStdString().c_str());
    if (ret < 0) {
        qDebug() << "解复用器打开失败";
        QString info = url + "打开失败";
        releaseResources();
        emit playFailed(info);
        return false;
    }

    file_url_ = url;

    hasAudio_ = demuxer_->hasStream(AVMEDIA_TYPE_AUDIO);
    hasVideo_ = demuxer_->hasStream(AVMEDIA_TYPE_VIDEO);

    // 视频解码器
    if(hasVideo_){
        video_decoder_ = new Decoder();
        video_decoder_->useHardware(hardware_enabled_);
        video_stream_idx_ = demuxer_->getStreamIndex(AVMEDIA_TYPE_VIDEO);
        AVStream* vStream = demuxer_->getStream(AVMEDIA_TYPE_VIDEO);
        ret = video_decoder_->init(vStream->codecpar,AVMEDIA_TYPE_VIDEO,decoder_type_);
        if(ret < 0){
            qDebug() << "视频解码器初始化失败！";
            releaseResources();
            return false;
        }

        initVideoModule();

    }
    // 音频解码器
    if(hasAudio_){
        audio_decoder_ = new Decoder();
        audio_stream_idx_ = demuxer_->getStreamIndex(AVMEDIA_TYPE_AUDIO);

        AVStream* aStream = demuxer_->getStream(AVMEDIA_TYPE_AUDIO);
        ret = audio_decoder_->init(aStream->codecpar,AVMEDIA_TYPE_AUDIO);
        if(ret < 0){
            qDebug() << "音频解码器初始化失败！";
            releaseResources();
            return false;
        }

        initAudioModule();
    }

    // 设置基准时钟
    AVSyncClock::SyncMode syncMode;
    if (hasAudio_ && hasVideo_) {
        syncMode = AVSyncClock::AUDIO_MASTER;   // 音视频：音频基准
    } else if (hasAudio_) {
        syncMode = AVSyncClock::VIDEO_MASTER;   // 纯音频：无同步
    } else if (hasVideo_) {
        syncMode = AVSyncClock::SYSTEM_MASTER;  // 纯视频：系统时钟基准
    } else {
        syncMode = AVSyncClock::AUDIO_MASTER;
    }
    sync_clock_->setSyncMode(syncMode, hasAudio_, hasVideo_);

    duration_ms_ = demuxer_->getDuration();
    state_ = Stopped;

    // 字幕
    if (asrEnabled_ && hasAudio_ && !asr_manager_->isModelPathEmpty()) {
        AVStream* as = demuxer_->getStream(AVMEDIA_TYPE_AUDIO);
        bool ok = asr_manager_->init(url, demuxer_->mediaType(), as);
        if(ok){
            asr_manager_->start();
        }
    }

    emit initFinished();
    return true;
}
void PlayerCore::initAudioModule()
{
    if (!audio_decoder_ || !audio_decoder_->codecCtx()) {
        qDebug() << "音频解码器上下文无效，初始化失败";
        return;
    }

    AVCodecContext* codec_ctx = audio_decoder_->codecCtx();
    Resampler::AudioSpec in_spec;

    // 填充输入参数
    in_spec.sampleRate  = codec_ctx->sample_rate;
    in_spec.sampleFmt   = codec_ctx->sample_fmt;
    in_spec.chs         = codec_ctx->ch_layout.nb_channels;
    in_spec.chLayout    = codec_ctx->ch_layout;
    in_spec.bytesPerSample = av_get_bytes_per_sample(in_spec.sampleFmt);

    // SDL 播放参数
    Resampler::AudioSpec out_spec;
    out_spec.sampleRate  = 48000;
    out_spec.chs         = 2;
    out_spec.sampleFmt   = AV_SAMPLE_FMT_S16;
    //out_spec.sampleFmt   = codec_ctx->sample_fmt;
    out_spec.bytesPerSample = av_get_bytes_per_sample(out_spec.sampleFmt);
    av_channel_layout_from_string(&out_spec.chLayout, "stereo");

    // 初始化 倍速滤镜
    audio_filter_ = new AudioFilter();
    if (audio_filter_->init(in_spec.sampleRate, in_spec.sampleFmt, in_spec.chs) < 0) {
        qDebug() << "音频倍速滤镜初始化失败";
        delete audio_filter_; audio_filter_ = nullptr;
        return;
    }

    // 初始化 SDL 音频输出
    audio_output_ = new AudioOutput(in_spec, out_spec, audio_frame_queue_,sync_clock_);
    if (audio_output_->Init() < 0) {
        qDebug() << "SDL 音频输出初始化失败";
        delete audio_filter_; audio_filter_ = nullptr;
        delete audio_output_; audio_output_ = nullptr;
        return;
    }

    audio_output_->setAudioTimebase(demuxer_->getStream(AVMEDIA_TYPE_AUDIO)->time_base);

    qDebug() << "=== 音频模块初始化完成 ===";
    qDebug() << "解码器输出参数: 采样率" << in_spec.sampleRate << "Hz, "
             << "格式" << av_get_sample_fmt_name((AVSampleFormat)in_spec.sampleFmt) << ", "
             << "声道数" << in_spec.chs;
    qDebug() << "SDL播放参数:    采样率" << out_spec.sampleRate << "Hz, "
             << "格式" << av_get_sample_fmt_name((AVSampleFormat)out_spec.sampleFmt) << ", "
             << "声道数" << out_spec.chs;
}


void PlayerCore::initVideoModule()
{
    if (!video_decoder_ || !video_decoder_->codecCtx()) {
        qDebug() << "视频解码器初始化失败，无法初始化视频模块";
        return;
    }

    // 原视频参数
    AVCodecContext* codec_ctx = video_decoder_->codecCtx();
    VideoConverter::VideoSpec in_spec;
    in_spec.width    = codec_ctx->width;
    in_spec.height   = codec_ctx->height;
    in_spec.pixFmt   = codec_ctx->pix_fmt;

    // 输出视频参数
    VideoConverter::VideoSpec out_spec;
    out_spec.width    = in_spec.width;
    out_spec.height   = in_spec.height;
    out_spec.pixFmt   = AV_PIX_FMT_RGBA;

    // 初始化视频转换器
    converter_ = new VideoConverter();
    if (converter_->init(in_spec, out_spec) < 0) {
        qDebug() << "视频转换器初始化失败";
        delete converter_; converter_ = nullptr;
    }

    qDebug() << "=== 视频模块初始化完成 ===";
    qDebug() << "视频参数:" << in_spec.width << "x" << in_spec.height
             << " 格式:" << av_get_pix_fmt_name(in_spec.pixFmt);
}


void PlayerCore::play()
{
    QMutexLocker lock(&mutex_);
    // 已在播放，直接返回
    if (state_ == Running) {
        return;
    }

    if (state_ == Paused) {
        state_ = Running;
        if (audio_output_) {
            audio_output_->resume();
        }
        if(sync_clock_){
            sync_clock_->resume();
        }
        cond_.wakeAll();
        emit stateChanged();
        qDebug() << "播放器：恢复播放";
        return;
    }

    if (state_ == Stopped) {
        is_exit_ = false;
        is_seek_ = false;
        state_ = Running;

        // 解复用线程
        demux_thread_ = QThread::create(&PlayerCore::demuxThreadFunc, this);
        demux_thread_->start();

        // 音频解码线程
        if (audio_decoder_) {
            audio_decode_thread_ = QThread::create(&PlayerCore::audioDecodeThreadFunc, this);
            audio_decode_thread_->start();
        }

        // 视频解码线程
        if (video_decoder_) {
            video_decode_thread_ = QThread::create(&PlayerCore::videoDecodeThreadFunc, this);
            video_decode_thread_->start();

            video_render_thread_ = QThread::create(&PlayerCore::videoRenderThreadFunc, this);
            video_render_thread_->start();
        }

        qDebug() << "播放器：开始播放";
    }

    emit stateChanged();
}

void PlayerCore::pause()
{
    QMutexLocker lock(&mutex_);
    if (state_ == Running) {
        state_ = Paused;
        if (audio_output_) {
            audio_output_->pause();
        }
        if(sync_clock_){
            sync_clock_->pause();
        }
        qDebug() << "播放器：已暂停";
    }
    emit stateChanged();
}

void PlayerCore::stop()
{
    if (state_ == Stopped) {
        return;
    }
    asr_manager_->stop();
    {
        QMutexLocker lock(&mutex_);
        is_exit_ = true;
        is_seek_ = false;
        cond_.wakeAll();
    }

    clearAllQueues();

    if (demux_thread_) {
        demux_thread_->quit();
        demux_thread_->wait();
        delete demux_thread_;
        demux_thread_ = nullptr;
    }
    if (audio_decode_thread_) {
        audio_decode_thread_->quit();
        audio_decode_thread_->wait();
        delete audio_decode_thread_;
        audio_decode_thread_ = nullptr;
    }
    if (video_decode_thread_) {
        video_decode_thread_->quit();
        video_decode_thread_->wait();
        delete video_decode_thread_;
        video_decode_thread_ = nullptr;
    }

    if (video_render_thread_) {
        video_render_thread_->quit();
        video_render_thread_->wait();
        delete video_render_thread_;
        video_render_thread_ = nullptr;
    }

    releaseResources();
    sync_clock_->reset();
    emit subtitleReady("");

    state_ = Stopped;
    duration_ms_ = 0;
    audio_stream_idx_ = -1;
    video_stream_idx_ = -1;
    hasAudio_ = false;
    hasVideo_ = false;
    emit stateChanged();

    qDebug() << "播放器：已停止，资源已释放";

}

void PlayerCore::setSpeed(int speedIndex)
{
    QMutexLocker lock(&mutex_);
    if (audio_filter_) {
        audio_filter_->setSpeedIndex((AudioFilter::SpeedIndex)speedIndex);
    }
    if (sync_clock_) {
        double ratio = getSpeedFromIndex(speedIndex);
        sync_clock_->setSpeedRatio(ratio);
    }
}

void PlayerCore::useHardware(bool isUse)
{
    hardware_enabled_ = isUse;
    if(video_decoder_){
        video_decoder_->useHardware(isUse);
    }
}

void PlayerCore::setDecodeType(const QString &decoder)
{
    decoder_type_ = decoder;
}

void PlayerCore::seek(int64_t pos_us)
{
    if (duration_ms_ <= 0 || pos_us < 0) return;

    {
        QMutexLocker lock(&mutex_);
        is_seek_ = true;
        state_ = Paused;
        cond_.wakeAll();
    }

    // 注意：先清空队列和解码器缓存，后seek，否则会卡顿
    clearAllQueues();
    if (video_decoder_) video_decoder_->flush();
    if (audio_decoder_) audio_decoder_->flush();

    int ret = demuxer_->seek(pos_us);
    if (ret < 0) qWarning() << "seek failed";

    sync_clock_->reset();
    sync_clock_->set_audio_clock(pos_us);

    {
        QMutexLocker lock(&mutex_);
        is_seek_ = false;
        state_ = Running;
        cond_.wakeAll();
    }
    asr_manager_->reset();
}

void PlayerCore::setScreenshotSavePath(const QString &savePath)
{
    screenshot_save_path_ = savePath;
}

void PlayerCore::takeScreenshot()
{
    need_screenshot_ = true;
}

void PlayerCore::setVolume(int val)
{
    if (audio_output_) {
        audio_output_->setVolume(val);
    }
}

void PlayerCore::setMute(bool mute)
{
    if (audio_output_) {
        audio_output_->setMute(mute);
    }
}

bool PlayerCore::isMute() const
{
    if (audio_output_) {
        return audio_output_->isMute();
    }

    return false;
}


void PlayerCore::setAsrEnabled(bool enabled)
{
    asrEnabled_ = enabled;
    if(state_ == State::Stopped) return;

    if (enabled) {
        if(asr_manager_->isModelPathEmpty()) {
            QString savedPath = ConfigManager::instance().getModelPath();
            if (!savedPath.isEmpty()) {
                setModelPath(savedPath);
            } else {
                qDebug() << "model path is empty, please set in settings";
                return;
            }
        }
        AVStream* as = demuxer_->getStream(AVMEDIA_TYPE_AUDIO);
        if(asr_manager_->init(file_url_, demuxer_->mediaType(), as)){
            asr_manager_->start();
        }
    } else {
        asr_manager_->stop();
        emit subtitleReady("");
    }
}

void PlayerCore::setModelPath(const QString &path)
{
    if(path.isEmpty()) return;
    model_path_ = path;
    asr_manager_->setModelPath(model_path_);
    // 立即在后台开始预加载模型（不会阻塞调用线程）
    asr_manager_->warmUp();
}

bool PlayerCore::isAsrEnabled() const
{
    return asrEnabled_;
}

void PlayerCore::demuxThreadFunc()
{
    qDebug() << "解复用线程启动";
    AVPacket* pkt = av_packet_alloc();

    while (!is_exit_)
    {
        //暂停/Seek 时线程等待
        mutex_.lock();
        if (state_ == Paused || is_seek_)
        {
            qDebug() << "demux thread paused";
            cond_.wait(&mutex_);
        }
        mutex_.unlock();

        if (is_exit_) break;

        // 背压：只有当「所有存在的流」队列都已满时才睡眠等待。
        // 不能像以前那样先串行等音频满、再等视频满——否则音频队列一满就
        // 把整个 demux 线程卡死，连带饿死视频包供给（seek 时尤其明显：
        // 视频解码要吞掉关键帧到目标帧之间的大量帧，视频包消耗极快，
        // 而音频包小、消费慢，音频队列很容易先满）。
        const bool hasA = demuxer_->hasStream(AVMEDIA_TYPE_AUDIO);
        const bool hasV = demuxer_->hasStream(AVMEDIA_TYPE_VIDEO);
        while (!is_exit_ && !is_seek_)
        {
            const bool audioFull = !hasA || audio_pkt_queue_->Size() >= MAX_AUDIO_PKT;
            const bool videoFull = !hasV || video_pkt_queue_->Size() >= MAX_VIDEO_PKT;
            // 任一仍有空间就继续读包，保证每条流都能持续被喂数据
            if (!videoFull || !audioFull) break;
            QThread::msleep(1);
        }

        av_packet_unref(pkt);
        int ret = demuxer_->readPacket(pkt);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF){
                qDebug() << "解复用完成：媒体文件读取完毕";
                emit playFinished();
            }
            else{
                qDebug() << "解复用读取包失败，错误码：" << ret;
            }
            is_exit_ = true;
            cond_.wakeAll();
            break;
        }

        const int audio_idx = demuxer_->getStreamIndex(AVMEDIA_TYPE_AUDIO);
        const int video_idx = demuxer_->getStreamIndex(AVMEDIA_TYPE_VIDEO);

        if (pkt->stream_index == audio_idx && audio_pkt_queue_)
        {
            audio_pkt_queue_->Push(pkt);
            //qDebug() << "apkt queue size " << audio_pkt_queue_->Size();
        }
        else if (pkt->stream_index == video_idx && video_pkt_queue_)
        {
            video_pkt_queue_->Push(pkt);
            //qDebug() << "vpkt queue size " << video_pkt_queue_->Size();
        }
        else
        {
            av_packet_unref(pkt);
        }
    }
    av_packet_free(&pkt);
    qDebug() << "解复用线程退出";
}


void PlayerCore::audioDecodeThreadFunc()
{
    qDebug() << "音频解码线程启动";

    AVFrame* decoded_frame = av_frame_alloc();
    AVFrame* filtered_frame = av_frame_alloc();
    AVStream* as = demuxer_->getStream(AVMEDIA_TYPE_AUDIO);

    while (!is_exit_)
    {
        mutex_.lock();
        if (state_ == Paused || is_seek_)
        {
            qDebug() << "audio decode thread paused";
            cond_.wait(&mutex_);
        }
        mutex_.unlock();

        if (is_exit_) break;

        while (audio_frame_queue_->Size() >= MAX_AUDIO_FRAME && !is_exit_ && !is_seek_) {
            QThread::msleep(5);
        }

        AVPacket* pkt = audio_pkt_queue_->Pop(10);
        if (!pkt) continue;


        // 解码
        int ret = audio_decoder_->decode(pkt, decoded_frame);
        GlobalPool::getPacketPool().recycle(pkt);

        if (ret == AVERROR(EAGAIN)) {
            continue;
        }

        if (ret == AVERROR_EOF) {
            qDebug() << "音频解码完成（EOF）";
            is_exit_ = true;
            cond_.wakeAll();
            break;
        }
        if (ret < 0) {
            qDebug() << "音频解码失败，错误码：" << ret;
            continue;
        }

        int64_t pts = (decoded_frame->pts == AV_NOPTS_VALUE) ? NAN : decoded_frame->pts;
        double duration = av_q2d(AVRational{ decoded_frame->nb_samples, decoded_frame->sample_rate });

        asr_manager_->sendAudioFrame(decoded_frame);
        // 倍速滤镜处理
        if (audio_filter_ && audio_filter_->isInitialized())
        {
            ret = audio_filter_->process(decoded_frame, filtered_frame);
            if (ret < 0)
            {
                av_frame_unref(filtered_frame);
                av_frame_move_ref(filtered_frame, decoded_frame);
            }
        }
        else
        {
            av_frame_move_ref(filtered_frame, decoded_frame);
        }

        //使用原来的pts
        filtered_frame->pts = pts;
        filtered_frame->duration = duration;

        audio_frame_queue_->Push(filtered_frame);
        if(!hasVideo_) {
            emit timeChanged();
            checkAndUpdateSubtitle();
        }
        //qDebug() << "aframe queue size " << audio_frame_queue_->Size();
        av_frame_unref(decoded_frame);
        av_frame_unref(filtered_frame);
    }

    av_frame_free(&decoded_frame);
    av_frame_free(&filtered_frame);
    qDebug() << "音频解码线程退出";
}


void PlayerCore::videoDecodeThreadFunc()
{
    qDebug() << "视频解码线程启动";
    AVFrame* decoded_frame = av_frame_alloc();
    AVFrame* rgb_frame = av_frame_alloc();
    bool use_rgb_test = false;
    while (!is_exit_)
    {
        // 处理暂停 / Seek 等待
        mutex_.lock();
        if (state_ == Paused || is_seek_)
        {
            qDebug() << "video decode thread paused";
            cond_.wait(&mutex_);
        }
        mutex_.unlock();

        if (is_exit_) break;

        while (video_frame_queue_->Size() >= MAX_VIDEO_FRAME && !is_exit_ && !is_seek_) {
            QThread::msleep(5);
        }
        //qDebug() << "video queue size : " << video_pkt_queue_->Size();
        AVPacket* pkt = video_pkt_queue_->Pop(10);
        if (!pkt) {
            //qDebug() << "pkt is null";
            continue;
        }
        //qDebug() << "vpkt queue poped(size:" << video_pkt_queue_->Size() << ")";
        int ret = video_decoder_->decode(pkt, decoded_frame);
        //av_packet_free(&pkt);
        GlobalPool::getPacketPool().recycle(pkt);

        if (ret == AVERROR(EAGAIN)) {
            continue;
        }
        if (ret == AVERROR_EOF) {
            qDebug() << "视频解码完成（EOF）";
            break;
        }
        if (ret < 0) {
            qDebug() << "视频解码失败，错误码：" << ret;
            continue;
        }

        if(use_rgb_test){
            ret = converter_->convert(decoded_frame, rgb_frame);

            if (ret < 0)
            {
                qDebug() << "Converter failed";
                continue;
            }
            video_frame_queue_->Push(rgb_frame);
            av_frame_unref(rgb_frame);

        }else{
            video_frame_queue_->Push(decoded_frame);
            av_frame_unref(decoded_frame);
        }

    }

    // 释放资源
    av_frame_free(&decoded_frame);
    av_frame_free(&rgb_frame);
    qDebug() << "视频解码线程退出";

}

void PlayerCore::videoRenderThreadFunc()
{
    qDebug() << "视频渲染线程启动";
    AVFrame* frame = nullptr;

    while (!is_exit_)
    {
        mutex_.lock();
        if (state_ == Paused || is_seek_) {
            cond_.wait(&mutex_);
        }
        mutex_.unlock();

        if (is_exit_) break;


        frame = video_frame_queue_->Pop(10);
        if (!frame) continue;

        // 视频pts
        AVStream* vs = demuxer_->getStream(AVMEDIA_TYPE_VIDEO);
        int64_t video_pts_us = av_rescale_q(frame->pts, vs->time_base, {1,1000000});
        qDebug() << "video pts : " << video_pts_us;

        // 计算延迟
        int64_t delay = sync_clock_->calc_display_delay(video_pts_us);
        //qDebug() << "video delay:" << delay;
        if (delay > 0) {
            QThread::usleep(delay);
            //qDebug() << "video thread sleeping:" << delay;
        }

        // 严重滞后，丢帧追赶
        if (sync_clock_->need_force_catch_up()) {
            GlobalPool::getFramePool().recycle(frame);
            continue;
        }

        int w = frame->width;
        int h = frame->height;
        AVPixelFormat fmt = (AVPixelFormat)frame->format;
        QByteArray frame_data;

        if (fmt == AV_PIX_FMT_YUV420P) {
            int y_size = w * h;
            int u_size = w * h / 4;
            int v_size = w * h / 4;
            frame_data.reserve(y_size + u_size + v_size);
            frame_data.append((const char*)frame->data[0], y_size);
            frame_data.append((const char*)frame->data[1], u_size);
            frame_data.append((const char*)frame->data[2], v_size);

            emit frameYuv420pDecoded(frame_data, w, h);
        }
        else if (fmt == AV_PIX_FMT_NV12) {
            // NV12: Y plane (w*h) + UV plane (w*h/2)
            int buf_size = w * h * 3 / 2;
            frame_data.reserve(buf_size);
            // 拷贝Y平面
            const uint8_t* y_buf = frame->data[0];
            int y_stride = frame->linesize[0];
            for (int i = 0; i < h; i++) {
                frame_data.append((const char*)y_buf, w);
                y_buf += y_stride;
            }
            // 拷贝UV交织平面
            const uint8_t* uv_buf = frame->data[1];
            int uv_stride = frame->linesize[1];
            int uv_h = h / 2;
            int uv_w = w / 2;
            for (int i = 0; i < uv_h; i++) {
                frame_data.append((const char*)uv_buf, uv_w * 2); // NV12是RG双通道，*2
                uv_buf += uv_stride;
            }

            emit frameNv12Decoded(frame_data, w, h);
        }else if (fmt == AV_PIX_FMT_RGBA || fmt == AV_PIX_FMT_BGRA) {
            int buf_size = w * h * 4;
            frame_data.reserve(buf_size);
            const uint8_t* data = frame->data[0];
            int stride = frame->linesize[0];

            for (int i = 0; i < h; i++) {
                frame_data.append((const char*)data, w * 4); // RGBA=4字节
                data += stride;
            }

            emit frameRGBADecoded(frame_data, w, h);
        }
        else {
            qWarning() << "不支持的视频格式：" << av_get_pix_fmt_name(fmt);
            GlobalPool::getFramePool().recycle(frame);
            continue;
        }

        if (need_screenshot_ && !screenshot_busy_) {
            need_screenshot_ = false;
            screenshot_busy_ = true;

            QByteArray frame_copy = frame_data;
            int w_copy = w;
            int h_copy = h;
            AVPixelFormat fmt_copy = fmt;
            QString path_copy = screenshot_save_path_;

            QtConcurrent::run([=]() {
                this->saveFrameToImage(frame_copy, w_copy, h_copy, fmt_copy, path_copy);
                screenshot_busy_ = false;
            });
        }

        emit timeChanged();
        checkAndUpdateSubtitle();
        GlobalPool::getFramePool().recycle(frame);
    }
    qDebug() << "视频渲染线程退出";
}


void PlayerCore::saveFrameToImage(const QByteArray& frame_data, int width, int height, AVPixelFormat format, const QString& savePath)
{
    if(frame_data.isEmpty() || width <=0 || height <=0)
        return;

    // 保存路径
    QString saveDir;
    if (!savePath.isEmpty())
    {
        saveDir = savePath;
    }
    else
    {
        // 全局通用：系统 图片 文件夹
        saveDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        saveDir += "/SmartPlayer_Screenshot";
    }

    QDir dir(saveDir);
    if (!dir.exists()) dir.mkpath(saveDir);

    QString fileName = QString("screenshot_%1.jpg").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hhmmss"));
    QString fullPath = dir.filePath(fileName);

    // 格式转换
    uint8_t* rgb_buf[4] = {nullptr};
    int rgb_linesize[4] = {0};
    int allocResult = av_image_alloc(rgb_buf, rgb_linesize, width, height, AV_PIX_FMT_RGB24, 1);
    if (allocResult < 0) {
        qWarning() << "av_image_alloc failed for screenshot";
        return;
    }

    SwsContext* sws_ctx = sws_getContext(
        width, height, format,
        width, height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
        );

    const uint8_t* src_data[4] = {nullptr};
    int src_linesize[4] = {0};
    if(format == AV_PIX_FMT_NV12){
        src_data[0] = (uint8_t*)frame_data.constData();
        src_linesize[0] = width;
        src_data[1] = (uint8_t*)frame_data.constData() + width*height;
        src_linesize[1] = width;
    }else{
        src_data[0] = (uint8_t*)frame_data.constData();
        src_linesize[0] = width;
        src_data[1] = (uint8_t*)frame_data.constData() + width*height;
        src_linesize[1] = width/2;
        src_data[2] = (uint8_t*)frame_data.constData() + width*height*5/4;
        src_linesize[2] = width/2;
    }

    sws_scale(sws_ctx, src_data, src_linesize, 0, height, rgb_buf, rgb_linesize);

    QImage img(rgb_buf[0], width, height, rgb_linesize[0], QImage::Format_RGB888);
    bool ok = img.save(fullPath, "JPG", 95);
    if(ok){
        qDebug() << "截图保存成功：" << fullPath;
        emit screecshotStatus(fullPath,true);
    }else{
        qDebug() << "截图保存失败：" << fullPath;
        emit screecshotStatus(fullPath,false);
    }


    sws_freeContext(sws_ctx);
    av_freep(&rgb_buf[0]);
    av_freep(&rgb_buf[1]);
    av_freep(&rgb_buf[2]);
    av_freep(&rgb_buf[3]);
}

double PlayerCore::getSpeedFromIndex(int speedIndex)
{
    switch (speedIndex) {
        case 1:  return 0.5;  // 0.5倍
        case 2:  return 1.0;  // 1倍（正常）
        case 3:  return 1.5;  // 1.5倍
        case 4:  return 2.0;  // 2倍
        default: return 1.0;
    }
}

void PlayerCore::checkAndUpdateSubtitle()
{
    if (state_ == Stopped || state_ == Paused) return;
    double now = currentTimeSec();
    SubtitleItem sub = asr_manager_->queue()->getCurrent(now);
    if (sub.text != current_display_sub_.text) {
        current_display_sub_ = sub;
        emit subtitleReady(QString::fromStdString(sub.text));
    }
}


void PlayerCore::clearAllQueues()
{
    if (audio_pkt_queue_)   audio_pkt_queue_->clear();
    if (video_pkt_queue_)   video_pkt_queue_->clear();
    if (audio_frame_queue_) audio_frame_queue_->clear();
    if (video_frame_queue_) video_frame_queue_->clear();

}

void PlayerCore::releaseResources()
{
    // 释放音频模块
    if (audio_filter_) {
        audio_filter_->close();
        delete audio_filter_;
        audio_filter_ = nullptr;
    }
    if (audio_output_) {
        delete audio_output_;
        audio_output_ = nullptr;
    }
    if (audio_decoder_) {
        audio_decoder_->close();
        delete audio_decoder_;
        audio_decoder_ = nullptr;
    }

    // 释放视频模块
    if (converter_) {
        delete converter_;
        converter_ = nullptr;
    }
    if (video_decoder_) {
        video_decoder_->close();
        delete video_decoder_;
        video_decoder_ = nullptr;
    }

    // 释放解复用器
    if (demuxer_) {
        demuxer_->close();
        delete demuxer_;
        demuxer_ = nullptr;
    }

    qDebug() << "资源已释放";
}

int64_t PlayerCore::duration() const
{
    return duration_ms_;
}

int64_t PlayerCore::currentPos() const
{
    if (hasAudio_) {
        // 音视频/纯音频
        return sync_clock_->get_audio_clock() / 1000000;
    } else if (hasVideo_) {
        // 纯视频
        return sync_clock_->getCurrentSystemClock() / 1000000;
    }
    return 0;
}

double PlayerCore::currentTimeSec() const
{
    if (hasAudio_)
        return sync_clock_->get_audio_clock() / 1000000.0;
    if (hasVideo_)
        return sync_clock_->getCurrentSystemClock() / 1000000.0;
    return 0.0;
}

Demuxer::MediaType PlayerCore::mediaType() const
{
    if(demuxer_) return demuxer_->mediaType();
    return Demuxer::MediaType::FILE_TYPE;
}

PlayerCore::State PlayerCore::state() const
{
    return state_;
}

AVFormatContext *PlayerCore::avFormatContext() const
{
    if(!demuxer_) return nullptr;
    return demuxer_->formatContext();
}

QString PlayerCore::fileUrl() const
{
    return file_url_;
}

bool PlayerCore::hasAudio() const
{
    return hasAudio_;
}

bool PlayerCore::hasVideo() const
{
    return hasVideo_;
}


