#define NOMINMAX
#include "asrpipeline.h"
#include "whisperengine.h"
#include "asrmodelcache.h"
#include "utils/asrutils.h"
#include <QDebug>
#include <chrono>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

AsrPipeline::AsrPipeline(QObject* parent) : QObject(parent) {}

AsrPipeline::~AsrPipeline() {
    stop();
}

bool AsrPipeline::init(const PipelineConfig& cfg, AVStream* audio, SubtitleQueue* queue) {
    config_ = cfg;
    queue_ = queue;

    // 初始化重采样器（如果有音频流参数）
    if (audio) {
        tb_ = audio->time_base;

        Resampler::AudioSpec in_spec, out_spec;
        in_spec.sampleRate = audio->codecpar->sample_rate;
        in_spec.sampleFmt = (AVSampleFormat)audio->codecpar->format;
        in_spec.chs = audio->codecpar->ch_layout.nb_channels;
        av_channel_layout_copy(&in_spec.chLayout, &audio->codecpar->ch_layout);

        out_spec.sampleRate = 16000;
        out_spec.sampleFmt = AV_SAMPLE_FMT_FLT;
        out_spec.chs = 1;
        av_channel_layout_from_string(&out_spec.chLayout, "mono");

        resampler_ = std::make_unique<Resampler>();
        if (resampler_->init(in_spec, out_spec)< 0) {
            emit engineError("Resampler init failed");
            return false;
        }
    }

    // 初始化 VAD（使用 AsrManager 注入的引擎实例，不重新加载模型）
    if (config_.enable_vad && vad_ && vad_->isReady()) {
        vad_->reset();   // 重置状态，复用已加载的模型
    } else if (config_.enable_vad) {
        qDebug() << "[AsrPipeline] VAD not ready, falling back to no-VAD mode";
        config_.enable_vad = false;
    }

    // 初始化 ASR 引擎（使用 AsrManager 注入的引擎实例，不重新加载模型）
    if (!asr_ || !asr_->isReady()) {
        emit engineError("ASR engine not ready");
        return false;
    }

    // 初始化翻译引擎（使用 AsrManager 注入的实例，可选）
    if (config_.enable_translation && translator_ && translator_->isReady()) {
        // 翻译引擎已就绪
    } else if (config_.enable_translation) {
        qDebug() << "[AsrPipeline] Translator not ready, translation disabled";
        config_.enable_translation = false;
    }

    return true;
}

void AsrPipeline::start() {
    if (running_.exchange(true)) return;

    // 根据音频源模式选择处理线程
    if (source_ && source_->mode() == AudioSourceMode::Pull) {
        vad_asr_thread_ = std::thread(&AsrPipeline::offlineLoop, this);
    } else {
        vad_asr_thread_ = std::thread(&AsrPipeline::vadAsrLoop, this);
    }

    if (config_.enable_translation && translator_ && translator_->isReady()) {
        translate_running_ = true;
        translate_thread_ = std::thread(&AsrPipeline::translateLoop, this);
    }
}

void AsrPipeline::stop() {
    running_ = false;
    if (vad_asr_thread_.joinable()) vad_asr_thread_.join();

    // 停止翻译线程
    translate_running_ = false;
    translate_cv_.notify_all();
    if (translate_thread_.joinable()) translate_thread_.join();

    // 清理
    ring_.clear();
    last_text_.clear();
    vad_pcm_buffer_.clear();
    vad_pcm_initialized_ = false;
    {
        std::lock_guard<std::mutex> lock(translate_mtx_);
        translate_queue_ = {};
    }
}

void AsrPipeline::reset() {
    ring_.clear();
    last_text_.clear();
    vad_pcm_buffer_.clear();
    vad_pcm_initialized_ = false;
    if (vad_) vad_->reset();
    if (asr_) asr_->reset();
    {
        std::lock_guard<std::mutex> lock(translate_mtx_);
        translate_queue_ = {};
    }
}

void AsrPipeline::setSource(std::unique_ptr<IAudioSource> source) {
    source_ = std::move(source);
}

void AsrPipeline::feedAudio(AVFrame* frame) {
    // 新架构：优先委托给音频源
    if (source_ && source_->mode() == AudioSourceMode::Push) {
   source_->pushFrame(frame);
        return;
    }

    // 兼容旧路径：直接使用内部 resampler + ring
    if (!resampler_ || !frame) return;

    uint8_t* buf = (uint8_t*)av_malloc(resampler_->outputBufferSize(frame->nb_samples));
    int samples = 0;
    if (resampler_->resample(frame, &buf, &samples) >= 0 && samples > 0) {
     double pts = frame->pts * av_q2d(tb_);
        ring_.push((float*)buf, samples, pts);
 }
    av_freep(&buf);
}

void AsrPipeline::feedPcm(const std::vector<float>& pcm, double base_sec) {
    if (!asr_ || !asr_->isReady()) return;

    if (config_.enable_vad && vad_ && vad_->isReady()) {
        processVadAudio(pcm, base_sec);
    } else {
        processDirectAsr(pcm, base_sec);
    }
}

void AsrPipeline::setAsrEngine(AsrEngineType type, const AsrEngineConfig& cfg) {
    // 引擎由 AsrManager 持有和注入，Pipeline 只更新配置
    config_.asr_type = type;
    config_.asr_config = cfg;
}

void AsrPipeline::setTranslator(TranslatorType type, const TranslateConfig& cfg) {
    // 翻译引擎由 AsrManager 持有和注入，Pipeline 只更新配置
    config_.translator_type = type;
    config_.translate_config = cfg;
}

void AsrPipeline::enableTranslation(bool enable) {
    config_.enable_translation = enable;

    if (enable && translator_ && translator_->isReady() && !translate_running_) {
        translate_running_ = true;
        translate_thread_ = std::thread(&AsrPipeline::translateLoop, this);
    } else if (!enable) {
        translate_running_ = false;
        translate_cv_.notify_all();
        if (translate_thread_.joinable()) translate_thread_.join();
    }
}

void AsrPipeline::enableVad(bool enable) {
    if (enable && (!vad_ || !vad_->isReady())) {
        qWarning() << "[AsrPipeline] Cannot enable VAD: injected engine is not ready";
        config_.enable_vad = false;
        return;
    }

    config_.enable_vad = enable;
    vad_pcm_buffer_.clear();
    vad_pcm_initialized_ = false;
    if (enable) vad_->reset();
}

std::string AsrPipeline::currentAsrEngineName() const {
    return asr_ ? asr_->name() : "None";
}

std::string AsrPipeline::currentTranslatorName() const {
    return translator_ ? translator_->name() : "None";
}

//========== 内部线程 ==========

void AsrPipeline::vadAsrLoop() {
    const int SR = 16000;
 const size_t win = 3* SR;    // 3s 窗口
    const size_t step = 1 * SR;   // 1s 步进
    std::vector<float> buf(win);

    // 判断数据源：优先使用 source_（Push 模式），否则回退到内部 ring_
    const bool use_source = source_ && source_->mode() == AudioSourceMode::Push;

    while (running_) {
        size_t avail = use_source ? source_->available() : ring_.available();
  if (avail < win) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
      continue;
        }

        double start_time;
     if (use_source) {
 start_time = source_->headTimeSec();
            source_->peek(buf.data(), win);
        } else {
     start_time = ring_.head_time_sec();
     ring_.peek(buf.data(), win);
   }

        if (config_.enable_vad && vad_ && vad_->isReady()) {
            processVadAudio(buf, start_time);
        } else {
            // 无VAD，直接做ASR
            processDirectAsr(buf, start_time);
        }

   if (use_source) {
        source_->consume(step);
        } else {
            ring_.consume(step);
        }
    }

    // 刷新 VAD 残余
    if (config_.enable_vad && vad_ && vad_->isReady()) {
        processVadSegments(vad_->flush());
    }
}

void AsrPipeline::processVadAudio(const std::vector<float>& pcm, double base_sec) {
    constexpr int SR = 16000;
    constexpr double kTimestampToleranceSec = 0.02;
    if (pcm.empty() || !vad_ || !vad_->isReady()) return;

    size_t skip = 0;
    if (!vad_pcm_initialized_) {
        vad_pcm_base_sec_ = base_sec;
        vad_pcm_initialized_ = true;
    } else {
        const double buffered_end = vad_pcm_base_sec_ +
            (double)vad_pcm_buffer_.size() / SR;
        const double delta = base_sec - buffered_end;
        if (delta > kTimestampToleranceSec) {
            qWarning() << "[AsrPipeline] VAD audio discontinuity:" << delta << "s, resetting";
            vad_->reset();
            vad_pcm_buffer_.clear();
            vad_pcm_base_sec_ = base_sec;
        } else if (delta < 0.0) {
            skip = (size_t)std::llround(-delta * SR);
            if (skip >= pcm.size()) return;
        }
    }

    const double new_audio_base = base_sec + (double)skip / SR;
    std::vector<float> new_audio(pcm.begin() + skip, pcm.end());
    vad_pcm_buffer_.insert(vad_pcm_buffer_.end(), new_audio.begin(), new_audio.end());

    auto segments = vad_->process(new_audio, new_audio_base);
    qDebug() << "[AsrPipeline] VAD audio @" << new_audio_base << "s"
             << "samples=" << new_audio.size()
             << "segments=" << segments.size();
    processVadSegments(segments);

    // FSMN 配置限制单段为 20 秒；保留 30 秒足以覆盖跨块闭合及静音拖尾。
    constexpr size_t kMaxBufferedSamples = 30 * SR;
    if (vad_pcm_buffer_.size() > kMaxBufferedSamples) {
        const size_t remove_count = vad_pcm_buffer_.size() - kMaxBufferedSamples;
        vad_pcm_buffer_.erase(vad_pcm_buffer_.begin(),
                              vad_pcm_buffer_.begin() + remove_count);
        vad_pcm_base_sec_ += (double)remove_count / SR;
    }
}

void AsrPipeline::processVadSegments(const std::vector<VadSegment>& segments) {
    constexpr int SR = 16000;

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];
        const int64_t start_sample = std::llround((seg.start_sec - vad_pcm_base_sec_) * SR);
        const int64_t end_sample = std::llround((seg.end_sec - vad_pcm_base_sec_) * SR);

        if (start_sample < 0 || end_sample > (int64_t)vad_pcm_buffer_.size() ||
            end_sample <= start_sample) {
            qWarning() << "[AsrPipeline] VAD segment outside PCM history:"
                       << seg.start_sec << "-" << seg.end_sec
                       << "buffer_base=" << vad_pcm_base_sec_
                       << "buffer_samples=" << vad_pcm_buffer_.size();
            continue;
        }

        std::vector<float> speech_pcm(vad_pcm_buffer_.begin() + start_sample,
                                      vad_pcm_buffer_.begin() + end_sample);

        std::vector<SubtitleItem> results;
        bool ok = asr_->recognize(speech_pcm, results, seg.start_sec);
        if (!ok) {
            qDebug() << "[AsrPipeline] ASR recognize failed for seg" << i;
            continue;
        }
        qDebug() << "[AsrPipeline] seg" << i
                 << "@" << seg.start_sec << "-" << seg.end_sec << "s"
                 << "pcm=" << speech_pcm.size() << "samples"
                 << "results=" << results.size();

        for (auto& item : results) {
            if (item.text != last_text_) {
                last_text_ = item.text;
                queue_->push(item);
                emit subtitleReady(item);

                if (config_.enable_translation && translator_) {
                    std::lock_guard<std::mutex> lock(translate_mtx_);
                    translate_queue_.push(item);
                    translate_cv_.notify_one();
                }
            }
        }
    }
}

void AsrPipeline::processDirectAsr(const std::vector<float>& pcm, double base_sec) {
    std::vector<SubtitleItem> results;
    bool ok = asr_->recognize(pcm, results, base_sec);
    qDebug() << "[AsrPipeline] directAsr @" << base_sec << "s"
             << "pcm=" << pcm.size() << "ok=" << ok << "results=" << results.size();
    if (!ok) return;
    std::string text;
    for (auto& i : results) text = AsrUtil::mergeOverlap(text, i.text);

    if (!text.empty() && text != last_text_) {
        last_text_ = text;
        SubtitleItem item;
        item.text = text;
        item.start_sec = results.front().start_sec;
        item.end_sec = results.back().end_sec;
        queue_->push(item);
        emit subtitleReady(item);

        // 送入翻译队列
        if (config_.enable_translation && translator_) {
            std::lock_guard<std::mutex> lock(translate_mtx_);
            translate_queue_.push(item);
            translate_cv_.notify_one();
        }
    }
}

void AsrPipeline::offlineLoop() {
    if (!source_) {
        qWarning() << "[AsrPipeline] offlineLoop: no source bound";
        running_ = false;
        return;
    }

#ifdef _WIN32
    // 降低识别线程优先级，避免抢占音视频解码/渲染线程
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif

    const int SR = 16000;
    // Whisper 设计处理 30s chunk；SenseVoice 等 ONNX 引擎建议 ≤10s
    const int CHUNK_SEC = (config_.asr_type == AsrEngineType::Whisper) ? 30 : 10;
    const int CHUNK_SAMPLES = CHUNK_SEC * SR;
    std::vector<float> pcm(CHUNK_SAMPLES);

    qDebug() << "[AsrPipeline] offlineLoop started:"
             << "chunk_sec=" << CHUNK_SEC
             << "vad=" << (config_.enable_vad && vad_ && vad_->isReady() ? "on" : "off")
             << "asr=" << currentAsrEngineName().c_str();

    //======= 自适应节流策略（Adaptive Throttling） =======
    //
    // 三阶段模型：
    //1. 追赶阶段（behind）：识别落后播放进度 → 尽快追赶，但基于 RTF 动态 yield
    //   2. 跟随阶段（tracking）：识别已追上播放 → 按播放速率匀速推进
    //   3. 超前阶段（ahead）：超前过多 → 暂停等待播放追上
    //
    // 核心指标：RTF（Real-Time Factor）= 处理耗时 / chunk 音频时长
    //RTF < 1 → 识别比实时快（正常），有余量
    //   RTF ≥ 1 → 识别比实时慢（压力大），需要减少 yield
    //
    // yield 时间 = max(chunk_duration * duty_cycle_factor, min_yield)
    // duty_cycle_factor根据阶段和RTF 动态调整

    const double chunk_duration_ms = CHUNK_SEC * 1000.0;
    const double lookahead_ms = lookahead_sec_ * 1000.0;

    // 平滑 RTF 估计（指数移动平均）
    double ema_rtf = 0.5;   // 初始假设 RTF=0.5
    const double ema_alpha = 0.3;   // 平滑系数

    while (running_ && !source_->isEof() && !source_->isCancelled()) {
        double media_time = 0.0;
        int n = source_->pull(pcm.data(), CHUNK_SAMPLES, media_time);
        if (n <= 0) break;

        // --- 超前等待（阶段 3）---
        // 识别进度大幅超前时，精确等待到播放追上，避免无谓 CPU 消耗
        if (playback_pos_fn_) {
            while (running_ && !source_->isCancelled()) {
                double pos = playback_pos_fn_();
                double ahead = media_time - pos;
                if (ahead <= lookahead_sec_) break;
                // 精确等待：sleep（超前量- lookahead）的一半，避免过冲
                int wait_ms = std::max(50, std::min(500, (int)((ahead - lookahead_sec_) * 500)));
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
            }
            if (!running_ || source_->isCancelled()) break;
        }

        // --- 识别处理 + 计时 ---
        auto t_start = std::chrono::steady_clock::now();

        const double actual_chunk_sec = (double)n / SR;
        std::vector<float> chunk(pcm.begin(), pcm.begin() + n);

        if (config_.enable_vad && vad_ && vad_->isReady()) {
            processVadAudio(chunk, media_time);
        } else {
            qDebug() << "[AsrPipeline] chunk @" << media_time << "s"
                     << "direct_asr (no VAD)";
            processDirectAsr(chunk, media_time);
        }

        auto t_end = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

        // --- 更新 RTF ---
        double current_rtf = elapsed_ms / (actual_chunk_sec * 1000.0);
        ema_rtf = ema_alpha * current_rtf + (1.0 - ema_alpha) * ema_rtf;

        // --- 自适应 yield（阶段 1/2）---
        // 判断当前所处阶段
        double ahead_sec = 0.0;
        if (playback_pos_fn_) {
            ahead_sec = media_time - playback_pos_fn_();
        }

        int yield_ms = 0;

        if (ahead_sec <= 0) {
            // 追赶阶段：识别落后于播放
            // yield 最小化：仅保证 OS 调度（1-2ms），靠线程优先级保护渲染
            yield_ms = 2;
        } else if (ahead_sec < lookahead_sec_) {
            // 跟随阶段：已追上但未超前太多
            // 匀速推进：yield = chunk 音频时长 - 处理耗时
            // 目标：让识别速率 ≈ 播放速率，保持 lookahead 稳定
            double target_pace_ms = actual_chunk_sec * 1000.0;
            double spare_ms = target_pace_ms - elapsed_ms;

            if (spare_ms > 0) {
                // RTF < 1 有余量：yield 部分余量（保留 30% 作为识别 buffer）
                yield_ms = (int)(spare_ms * 0.7);
            } else {
                // RTF ≥ 1 无余量：最小yield
                yield_ms = 2;
            }
        }
        // ahead_sec >= lookahead_sec_ 的情况在循环头部的超前等待中已处理

        if (yield_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(yield_ms));
        }
    }

    if (config_.enable_vad && vad_ && vad_->isReady() &&
        source_->isEof() && !source_->isCancelled()) {
        processVadSegments(vad_->flush());
    }

    running_ = false;
}

void AsrPipeline::translateLoop() {
    while (translate_running_) {
        SubtitleItem item;
        {
            std::unique_lock<std::mutex> lock(translate_mtx_);
            translate_cv_.wait(lock, [this] {
                return !translate_queue_.empty() || !translate_running_;
            });
            if (!translate_running_) break;
            if (translate_queue_.empty()) continue;
            item = translate_queue_.front();
            translate_queue_.pop();
        }

        if (!translator_ || !translator_->isReady()) continue;

        auto result = translator_->translate(item.text);
        if (result.success) {
            item.translated_text = result.translated_text;
            // 更新字幕队列中的翻译结果
            queue_->push(item);
            emit translationReady(item);
        } else {
            qDebug() << "[AsrPipeline] Translation failed:" << QString::fromStdString(result.error_msg);
        }
    }
}
