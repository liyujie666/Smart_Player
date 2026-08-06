#include "fsmnvad.h"
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <numeric>

// ORT Context：封装 ONNX Runtime 会话相关资源
// 需要依赖 onnxruntime库
struct FsmnVad::OrtContext {
    // TODO: 实际集成时填充 onnxruntime session 等
    //Ort::Env env;
    // Ort::Session session;
    // Ort::MemoryInfo memory_info;
    bool valid = false;
};

FsmnVad::FsmnVad() = default;

FsmnVad::~FsmnVad() {
    release();
}

bool FsmnVad::init(const VadConfig& cfg) {
    cfg_ = cfg;

    // 帧参数
    frame_size_ = cfg_.sample_rate * 25 / 1000;// 25ms
    frame_shift_ = cfg_.sample_rate * 10 / 1000;  // 10ms

    ort_ctx_ = std::make_unique<OrtContext>();

    // TODO: 加载 ONNX 模型
    // Ort::SessionOptions session_options;
    // session_options.SetIntraOpNumThreads(2);
    // ort_ctx_->session = Ort::Session(ort_ctx_->env, cfg_.model_path.c_str(), session_options);

    // 初始化 FSMN 隐藏状态缓存
    // cache_大小由模型结构决定，典型为 [cache_dim] float
    cache_.resize(128* 4, 0.0f);

    state_ = State::Silence;
    silence_frame_count_ = 0;
    total_frames_processed_ = 0;
    residual_pcm_.clear();

    ready_ = true;  // TODO: 模型加载成功后设为 true
    qDebug() << "[FsmnVad] initialized, frame_size=" << frame_size_
             << "frame_shift=" << frame_shift_;
    return ready_;
}

void FsmnVad::release() {
    ort_ctx_.reset();
    cache_.clear();
    ready_ = false;
}

void FsmnVad::reset() {
    state_ = State::Silence;
    silence_frame_count_ = 0;
    total_frames_processed_ = 0;
    residual_pcm_.clear();
    completed_segments_.clear();
    std::fill(cache_.begin(), cache_.end(), 0.0f);
}

std::vector<VadSegment> FsmnVad::process(const std::vector<float>& pcm, double base_sec) {
    completed_segments_.clear();

    // 将残余数据与新数据拼接
    std::vector<float> audio;
    if (!residual_pcm_.empty()) {
        audio = std::move(residual_pcm_);
        audio.insert(audio.end(), pcm.begin(), pcm.end());
    } else {
        audio = pcm;
        residual_base_sec_ = base_sec;
    }

    // 逐帧处理
    size_t pos = 0;
    while (pos + frame_size_ <= audio.size()) {
        std::vector<float> frame_data(audio.begin() + pos, audio.begin() + pos + frame_size_);

        float speech_prob = inferFrame(frame_data);

        double frame_time = residual_base_sec_ +
            (double)(total_frames_processed_ * frame_shift_) / cfg_.sample_rate;

        updateState(speech_prob, frame_time);

        total_frames_processed_++;
        pos += frame_shift_;
    }

    // 保存残余
    if (pos < audio.size()) {
        residual_pcm_.assign(audio.begin() + pos, audio.end());
    } else {
        residual_pcm_.clear();
    }

    return std::move(completed_segments_);
}

std::vector<VadSegment> FsmnVad::flush() {
    completed_segments_.clear();

    // 如果当前正在语音段内，强制结束
    if (state_ == State::Speech || state_ == State::Trailing) {
        double end_time = residual_base_sec_ +
            (double)(total_frames_processed_ * frame_shift_) / cfg_.sample_rate;
        double duration_ms = (end_time - current_speech_start_) * 1000.0;

        if (duration_ms >= cfg_.min_speech_ms) {
            VadSegment seg;
            seg.start_sec = current_speech_start_;
            seg.end_sec = end_time;
            completed_segments_.push_back(seg);
        }
        state_ = State::Silence;
    }

    return std::move(completed_segments_);
}

float FsmnVad::inferFrame(const std::vector<float>& frame_data) {
    // TODO: 实际ONNX Runtime 推理
    // 输入：frame_data + cache_
    // 输出：speech_prob (0~1),更新 cache_
    //
    // 临时实现：基于能量的简单 VAD（占位）
    float energy = 0.0f;
    for (float s : frame_data) {
        energy += s * s;
    }
    energy = energy / frame_data.size();

    // 简单能量阈值映射为概率
    float db = 10.0f * std::log10(std::max(energy, 1e-10f));
    float prob = 1.0f / (1.0f + std::exp(-(db + 40.0f) * 0.3f));

    return prob;
}

void FsmnVad::updateState(float speech_prob, double frame_time) {
    bool is_speech = speech_prob >= cfg_.threshold;
    int silence_frames_threshold = cfg_.min_silence_ms * cfg_.sample_rate / (1000 * frame_shift_);

    switch (state_) {
    case State::Silence:
        if (is_speech) {
            state_ = State::Speech;
            current_speech_start_ = frame_time;
            silence_frame_count_ = 0;
        }
        break;

    case State::Speech:
        if (!is_speech) {
            state_ = State::Trailing;
            silence_frame_count_ = 1;
            current_speech_end_ = frame_time;
        }
        break;

    case State::Trailing:
        if (is_speech) {
            // 回到语音状态
            state_ = State::Speech;
            silence_frame_count_ = 0;
        } else {
            silence_frame_count_++;
            if (silence_frame_count_ >= silence_frames_threshold) {
                // 确认这段语音结束
                double duration_ms = (current_speech_end_ - current_speech_start_) * 1000.0;
                if (duration_ms >= cfg_.min_speech_ms) {
                    VadSegment seg;
                    seg.start_sec = current_speech_start_;
                    seg.end_sec = current_speech_end_;
                    completed_segments_.push_back(seg);
                }
                state_ = State::Silence;
            }
        }
        break;
    }
}
