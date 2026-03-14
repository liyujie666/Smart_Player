#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H
#include"condmutex.h"
#include <QObject>
#include <list>
#include <QMutex>
#include <algorithm>
extern"C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/imgutils.h>
#include <libavutil/buffer.h>
#include <libavutil/opt.h>
#include <libavutil/hwcontext.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
}

#define ERROR_BUF \
    char errbuf[1024];\
    av_strerror(ret,errbuf,sizeof(errbuf));

#define CODE(func,code) \
    if(ret < 0){ \
        ERROR_BUF; \
        qDebug() << #func << "error: " << errbuf; \
        code; \
    }

#define END(func) CODE(func, fataError(); return;)
#define RET(func) CODE(func, return ret;)
#define CONTINUE(func) CODE(func, continue;)
#define BREAK(func) CODE(func, break;)



class VideoPlayer : public QObject
{
    Q_OBJECT
public:
    //播放器的状态
    typedef enum
    {
        Stopped = 0,
        Playing,
        Paused
    } State;

    //音量
    typedef enum
    {
        Min = 0,
        Max = 100
    } Volume;

    typedef enum
    {
        FILE,
        RTSP,
        RTMP
    }Type;
    //frame参数
    typedef struct
    {
        int width;
        int height;
        enum AVPixelFormat pixFmt;
        int size;
    } VideoSwsSpec;



    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();

    void play();
    void play_preview();
    void pause();
    void stop();
    void stopwithSignal();
    bool isPlaying();
    void setState(State state); //改变状态
    State getState();
    int getDuration();
    int getTime();              //获取播放时间
    void setTime(int seekTime); //设置播放时间
    void setVolume(int volume);
    int getVolume();
    void setMute(bool mute);    //设置静音
    bool isMute();
    void updateSignal();
    void setSpeed(int index);   //设置播放速度
    AVFormatContext* getAVFormatContext();
    void setFilename(QString &filename);
    char* getFilename();
    Type getFileType();
    void readFile();            //读取文件数据
    void startPreview();        //用于预览的函数，仅有视频流
    int initDecoder(AVCodecContext **decodecCtx,AVStream **stream,AVMediaType type); //初始化解码器和解码器上下文
    void setRootFilePath(const QString &path);            //设置文件保存路径
    void requestSnapshot(const QString &filename);       //截图并设置文件名
    void setIsHardWare(bool on);                         //设置硬件加速
    void setDecodeType(const QString &newDecodeType);     //指定解码器

    void free(); //释放资源
    void freeAudio();
    void freeVideo();
    void fataError();//错误处理


    ;

signals:
    void stateChanged(VideoPlayer *player);
    void timeChanged(VideoPlayer *player);
    void initFinished(VideoPlayer *player);
    void playFailed(VideoPlayer *player);
    void frameDecoded(VideoPlayer *player,uint8_t *data,VideoSwsSpec &spec);
    void snapshotReady(AVFrame *frame, const VideoSwsSpec &spec, const QString &path);
    void showMessage(const QString &info,int interval);

private:
    //VideoPlayer参数
    CondMutex previewMutex_;
    QMutex demuxMutex_;
    AVFormatContext *fmtCtx_ = nullptr; //解封装上下文,预览线程解封装上下文
    bool fmtCtxCanFree_ = false;        //ftCtx是否可以释放
    int volume_ = Max;                  //音量
    bool mute_ = false;                 //静音
    State state_ = Stopped;             //当前的播放器状态
    char filename_[512];                //文件名
    int seekTime_ = -1;                 //外面设置的当前播放时刻
    int open(const char* fileName);    //打开文件
    Type fileType_;

    //音频参数
    typedef struct
    {
        int sampleRate;
        enum AVSampleFormat sampleFmt;
        AVChannelLayout chLayout;
        int chs;
        int bytesPerSampleFmt;
    } AudioSwrSpec;

    AVCodecContext* aCodecCtx_ = nullptr;   //音频解码器上下文
    AVStream* aStream_ = nullptr;            //音频流
    std::list<AVPacket*> aPktList_;         //音频包队列
    CondMutex aMutex_;                      //音频队列锁
    struct SwrContext* aSwrCtx_ = nullptr;        //音频重采样上下文
    AudioSwrSpec aSwrInSpec_,aSwrOutSpec_;//音频重采样输入、输出参数
    AVFrame *aSwrInFrame_ = nullptr,*aSwrOutFrame_ = nullptr;//重采样frame输入、输出
    int aSwrOutIndex_ = 0;              //音频重采样输出PCM的索引
    int aSwrOutSize_ = 0;               //音频重采样输出PCM的大小
    double aTime_ = 0;                  //音频时钟，当前音频包对应的时间值
    bool aCanFree_ = false;             //音频资源是否可以释放
    int aSeekTime_ = -1;                //外面设置的当前播放时刻
    bool hasAudio_ = false;             //是否有音频流

    //音频函数
    int initAudioInfo();                //初始化音频参数
    int initSwr();                      //初始化音频重采样
    int initSDL();                      //初始化SDL
    void addAudioPkt(AVPacket *pkt);    //将音频包入队
    void clearAudioPktList();           //清空音频包队列
    static void sdlAudioCallbackFunc(void *userdata, Uint8 *stream, int len);//SDL填充缓冲区的回调函数
    void sdlAudioCallback(Uint8 *stream, int len);//SDL填充缓冲区的回调函数
    int decodeAudio();                  //音频解码


    //视频参数
    AVCodecContext *vCodecCtx_ = nullptr; //视频解码器上下文
    AVStream *vStream_ = nullptr; //视频流
    std::list<AVPacket*> vPktList_; //视频包队列
    CondMutex vMutex_;              //视频队列锁
    struct SwsContext *vSwsCtx_ = nullptr; //像素格式转换的上下文
    VideoSwsSpec vSwsOutSpec_;      //像素格式转换的输出frame的参数
    AVFrame *vSwsInFrame_ = nullptr,*vSwsOutFrame_ = nullptr; //像素格式转换的输入、输出frame
    double vTime_ = 0;              //视频时钟，当前视频包对应的时间值
    bool vCanFree_ = false;         //视频资源是否可以释放
    int vSeekTime_ = -1;            //外面设置的当前播放时刻
    bool hasVideo_ = false;         //是否有视频流
    int frameRate_ = 25;            //视频帧率
    int frameDurationMs_ = 40;      //休眠时间
    QString decodeType_;            //用户指定解码器
    std::atomic_bool isReady{false};


    //视频函数
    int initVideoInfo();            //初始化视频信息
    int initSws();                  //初始化视频像素格式转换
    void addVideoPkt(AVPacket *pkt); //将视频包入队
    void clearVideoPktList();       //清空视频包队列
    void decodeVideo(bool isPreview);             //解码视频
    void decodeVideoNoAudio();
    void receiveVideoFrames();


    //视频截图
    std::atomic_bool takeSnapshot_{false};  // 是否需要截图
    QString snapshotPath_;       // 截图保存路径
    QString rootFilePath_ = "D:/Qt/QtProjects/ffmpegProjects/Smart_Player/screenshot/";

    //硬件加速
    bool useHwAccel_ = false;
    AVBufferRef* hwDeviceCtx_ = nullptr;
    AVFrame* swFrame_ = nullptr,*tmpFrame_ = nullptr;   // 用于硬解数据转CPU
    enum AVHWDeviceType hwType_ = AV_HWDEVICE_TYPE_CUDA;
    enum AVPixelFormat hw_pix_fmt_;


    //直接保存四个过滤器（0.5\1.0\1.5\2.0)
    int currentSpeedIndex=2;
    AVFilterGraph *graph_1 = nullptr;//过滤器链接图
    AVFilterContext *srcFilterCtx_1 = nullptr, *sinkFilterCtx_1 = nullptr;//过滤器源和接收过滤器上下文

    AVFilterGraph *graph_2 = nullptr;//过滤器链接图
    AVFilterContext *srcFilterCtx_2 = nullptr, *sinkFilterCtx_2 = nullptr;//过滤器源和接收过滤器上下文

    AVFilterGraph *graph_3 = nullptr;//过滤器链接图
    AVFilterContext *srcFilterCtx_3 = nullptr, *sinkFilterCtx_3 = nullptr;//过滤器源和接收过滤器上下文

    AVFilterGraph *graph_4 = nullptr;//过滤器链接图
    AVFilterContext *srcFilterCtx_4 = nullptr, *sinkFilterCtx_4 = nullptr;//过滤器源和接收过滤器上下文

    int initFilter(AVFilterGraph **graph, AVFilterContext **srcFilterCtx, AVFilterContext **sinkFilterCtx, char *value);//初始化过滤器
    void freeFilter();//释放过滤器

};

#endif // VIDEOPLAYER_H
