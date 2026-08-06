#ifndef ASRPIPELINE_H
#define ASRPIPELINE_H

#include <QObject>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

#include "ivadengine.h"
#include "iasrengine.h"
#include "itranslator.h"
#include "queue/subtitlequeue.h"
#include "resampler/resampler.h"
#include "utils/audioringbuffer.h"

extern "C" {
#include <libavformat/avformat.h>
}

// 管线配置
struct PipelineConfig {
    // VAD
    bool enable_vad = true;
    VadEngineType vad_type = VadEngineType::FSMN;
    VadConfig vad_config;

    // ASR
    AsrEngineType asr_type = AsrEngineType::Whisper;
    AsrEngineConfig asr_config;

    // 翻译
    bool enable_translation = false;
    TranslatorType translator_type = TranslatorType::GPT;
    TranslateConfig translate_config;
};

// 核心管线：VAD → ASR → Translator
// 统一编排音频处理流程，供离线/实时策略共用
class AsrPipeline : public QObject {
    Q_OBJECT
public:
    explicit AsrPipeline(QObject* parent = nullptr);
    ~AsrPipeline() override;

    bool init(const PipelineConfig& cfg, AVStream* audio, SubtitleQueue* queue);
    void start();
    void stop();
    void reset();

    // 实时模式：外部送入解码后的音频帧
    void feedAudio(AVFrame* frame);

    // 离线模式：送入整段PCM（已重采样16kHz/mono/float32）
    void feedPcm(const std::vector<float>& pcm, double base_sec);

    // 动态切换引擎（运行时）
    void setAsrEngine(AsrEngineType type, const AsrEngineConfig& cfg);
    void setTranslator(TranslatorType type, const TranslateConfig& cfg);
    void enableTranslation(bool enable);
    void enableVad(bool enable);

    // 获取当前引擎信息
    std::string currentAsrEngineName() const;
    std::string currentTranslatorName() const;

signals:
    void subtitleReady(const SubtitleItem& item);
    void translationReady(const SubtitleItem& item);
    void engineError(const QString& error);

private:
    void vadAsrLoop();       // VAD + ASR 处理线程
    void translateLoop();    // 翻译处理线程

    // 内部：对VAD输出的语音段进行ASR
    void processVadSegments(const std::vector<VadSegment>& segments,
                            const std::vector<float>& pcm,
                            double base_sec);

    // 内部：直接做ASR（无VAD时的回退路径）
    void processDirectAsr(const std::vector<float>& pcm, double base_sec);

private:
    PipelineConfig config_;
    SubtitleQueue* queue_ = nullptr;

    // 重采样
    std::unique_ptr<Resampler> resampler_;

    // 音频缓冲（实时模式用）
    AudioPcmRingBuffer ring_;
    AVRational tb_{0, 0};

    // 三大引擎
    std::unique_ptr<IVadEngine> vad_;
    std::unique_ptr<IAsrEngine> asr_;
    std::unique_ptr<ITranslator> translator_;

    // VAD + ASR 线程
    std::thread vad_asr_thread_;
    std::atomic<bool> running_{false};

    // 翻译线程（异步，不阻塞字幕原文显示）
    std::thread translate_thread_;
    std::atomic<bool> translate_running_{false};
    std::queue<SubtitleItem> translate_queue_;
    std::mutex translate_mtx_;
    std::condition_variable translate_cv_;

    // 实时模式去重
    std::string last_text_;

    // Whisper缓存模型管理标记
    bool uses_cached_model_ = false;
};

#endif // ASRPIPELINE_H
