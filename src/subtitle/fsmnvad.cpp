#include "fsmnvad.h"
#if HAS_ONNXRUNTIME
#include "utils/onnxruntimeutil.h"
#endif
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#if HAS_ONNXRUNTIME

FsmnVad::FsmnVad() = default;
FsmnVad::~FsmnVad() { release(); }

bool FsmnVad::loadModel(const std::string& model_dir) {
    QString dir = QString::fromStdString(model_dir);
    QString model_path = dir + "/model.onnx";
    QString mvn_path = dir + "/am.mvn";

    // 检查文件存在
    if (!QFileInfo::exists(model_path)) {
        qWarning() << "[FsmnVad] model.onnx not found:" << model_path;
        return false;
    }

    // 创建 ONNX Runtime session
    ort_ctx_ = std::make_unique<OrtContext>();
    auto& opts = ort_ctx_->session_options = std::make_unique<Ort::SessionOptions>(
        OrtUtil::defaultSessionOptions(2));
    ort_ctx_->session = std::make_unique<Ort::Session>(
        OrtUtil::instance().env(), model_path.toUtf8().constData(), *opts);
    ort_ctx_->memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    // 读取输入输出名称
    size_t n_inputs = ort_ctx_->session->GetInputCount();
    size_t n_outputs = ort_ctx_->session->GetOutputCount();
    Ort::AllocatorWithDefaultOptions alloc;

    for (size_t i = 0; i < n_inputs; ++i) {
        ort_ctx_->input_name_ptrs.push_back(
            ort_ctx_->session->GetInputNameAllocated(i, alloc));
        ort_ctx_->input_names.push_back(ort_ctx_->input_name_ptrs.back().get());
    }
    for (size_t i = 0; i < n_outputs; ++i) {
        ort_ctx_->output_name_ptrs.push_back(
            ort_ctx_->session->GetOutputNameAllocated(i, alloc));
        ort_ctx_->output_names.push_back(ort_ctx_->output_name_ptrs.back().get());
    }

    // 读取输入 shape，推断 chunk_size 和 cache_dim
    auto input_shapes = ort_ctx_->session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if (input_shapes.size() >= 2) {
        ort_ctx_->chunk_size = (input_shapes[1] > 0) ? input_shapes[1] : 2560;
    } else {
        ort_ctx_->chunk_size = 2560; // 默认 160ms
    }

    // 如果有第二个输入（cache），读取其维度
    if (n_inputs >= 2) {
        auto cache_shapes = ort_ctx_->session->GetInputTypeInfo(1).GetTensorTypeAndShapeInfo().GetShape();
        if (cache_shapes.size() >= 2) {
            ort_ctx_->cache_dim = (cache_shapes[1] > 0) ? cache_shapes[1] : 128;
            ort_ctx_->cache_shape_0 = (cache_shapes[0] > 0) ? cache_shapes[0] : 1;
        }
    }

    chunk_samples_ = (int)ort_ctx_->chunk_size;
    cache_.resize(ort_ctx_->cache_dim * ort_ctx_->cache_shape_0, 0.0f);

    qDebug() << "[FsmnVad] Model loaded:"
             << "chunk_size=" << chunk_samples_
             << "cache_dim=" << ort_ctx_->cache_dim
             << "n_inputs=" << n_inputs
             << "n_outputs=" << n_outputs;

    // 加载 CMVN
    if (QFileInfo::exists(mvn_path)) {
        auto cmvn = OrtUtil::loadCmvn(mvn_path.toStdString());
        cmvn_mean_ = std::move(cmvn.mean);
        cmvn_var_ = std::move(cmvn.variance);
    }

    return true;
}

bool FsmnVad::init(const VadConfig& cfg) {
    cfg_ = cfg;

    if (!loadModel(cfg_.model_path)) {
        qWarning() << "[FsmnVad] Failed to load model from" << QString::fromStdString(cfg_.model_path);
        return false;
    }

    state_ = State::Silence;
    silence_frame_count_ = 0;
    total_frames_processed_ = 0;
    residual_pcm_.clear();
    std::fill(cache_.begin(), cache_.end(), 0.0f);

    ready_ = true;
    qDebug() << "[FsmnVad] initialized, chunk_samples=" << chunk_samples_;
    return ready_;
}

void FsmnVad::release() {
    ort_ctx_.reset();
    cache_.clear();
    cmvn_mean_.clear();
    cmvn_var_.clear();
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

float FsmnVad::inferChunk(const std::vector<float>& chunk) {
    if (!ort_ctx_ || chunk.empty()) return 0.0f;

    int64_t chunk_len = (int64_t)chunk.size();

    // 输入: speech (1, chunk_len)
    std::vector<int64_t> speech_shape = {1, chunk_len};
    Ort::Value speech_tensor = Ort::Value::CreateTensor<float>(
        *ort_ctx_->memory_info, const_cast<float*>(chunk.data()), chunk_len, speech_shape.data(), speech_shape.size());

    // 输入: cache (1, cache_dim) 或 (1, cache_dim, 1)
    std::vector<int64_t> cache_shape;
    if (ort_ctx_->cache_shape_0 > 0) {
        cache_shape = {ort_ctx_->cache_shape_0, ort_ctx_->cache_dim, 1};
    } else {
        cache_shape = {1, ort_ctx_->cache_dim};
    }
    Ort::Value cache_tensor = Ort::Value::CreateTensor<float>(
        *ort_ctx_->memory_info, cache_.data(), cache_.size(), cache_shape.data(), cache_shape.size());

    // 组装输入
    std::vector<Ort::Value> inputs;
    inputs.push_back(std::move(speech_tensor));
    if (ort_ctx_->input_names.size() >= 2) {
        inputs.push_back(std::move(cache_tensor));
    }

    // 推理
    auto outputs = ort_ctx_->session->Run(
        Ort::RunOptions{nullptr},
        ort_ctx_->input_names.data(),
        inputs.data(),
        inputs.size(),
        ort_ctx_->output_names.data(),
        ort_ctx_->output_names.size());

    // 输出[0]: logits (1, num_frames, 2)
    // 输出[1]: cache (更新后的缓存)
    if (outputs.empty()) return 0.0f;

    // 更新 cache
    if (outputs.size() >= 2) {
        auto& cache_out = outputs[1];
        float* cache_data = cache_out.GetTensorMutableData<float>();
        size_t cache_count = cache_out.GetTensorTypeAndShapeInfo().GetElementCount();
        std::copy(cache_data, cache_data + cache_count, cache_.begin());
    }

    // 取最后一帧的 speech prob
    auto& logits = outputs[0];
    auto shape = logits.GetTensorTypeAndShapeInfo().GetShape();
    if (shape.size() < 3) return 0.0f;

    int64_t num_frames = shape[1];
    if (num_frames == 0) return 0.0f;

    float* logits_data = logits.GetTensorMutableData<float>();
    // logits shape: (1, num_frames, 2)
    // 最后一帧: [num_frames-1, 0] = non-speech, [num_frames-1, 1] = speech
    float non_speech = logits_data[(num_frames - 1) * 2 + 0];
    float speech = logits_data[(num_frames - 1) * 2 + 1];

    // softmax → 概率
    float max_val = std::max(non_speech, speech);
    float exp_ns = std::exp(non_speech - max_val);
    float exp_s = std::exp(speech - max_val);
    float prob = exp_s / (exp_ns + exp_s);

    return prob;
}

std::vector<VadSegment> FsmnVad::process(const std::vector<float>& pcm, double base_sec) {
    completed_segments_.clear();

    // 拼接残余数据
    std::vector<float> audio;
    if (!residual_pcm_.empty()) {
        audio = std::move(residual_pcm_);
        audio.insert(audio.end(), pcm.begin(), pcm.end());
    } else {
        audio = pcm;
        residual_base_sec_ = base_sec;
    }

    // 按 chunk_size 逐块处理
    size_t pos = 0;
    while (pos + chunk_samples_ <= audio.size()) {
        std::vector<float> chunk(audio.begin() + pos, audio.begin() + pos + chunk_samples_);

        float speech_prob = inferChunk(chunk);

        // 每个 chunk 输出 1 帧（10ms 粒度）
        double frame_time = residual_base_sec_ +
            (double)(total_frames_processed_ * frame_shift_ms_) / 1000.0;

        updateState(speech_prob, frame_time);

        total_frames_processed_++;
        pos += chunk_samples_;
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

    if (state_ == State::Speech || state_ == State::Trailing) {
        double end_time = residual_base_sec_ +
            (double)(total_frames_processed_ * frame_shift_ms_) / 1000.0;
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

void FsmnVad::updateState(float speech_prob, double frame_time) {
    bool is_speech = speech_prob >= cfg_.threshold;
    int silence_frames_threshold = cfg_.min_silence_ms / frame_shift_ms_;

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
            state_ = State::Speech;
            silence_frame_count_ = 0;
        } else {
            silence_frame_count_++;
            if (silence_frames_threshold <= 0) silence_frames_threshold = 30; // 默认 300ms
            if (silence_frame_count_ >= silence_frames_threshold) {
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

#else // !HAS_ONNXRUNTIME

// ========== 无 ONNX Runtime 的空实现 ==========

FsmnVad::FsmnVad() = default;
FsmnVad::~FsmnVad() { release(); }

bool FsmnVad::init(const VadConfig& cfg) {
    qWarning() << "[FsmnVad] Built without ONNX Runtime support. VAD disabled.";
    (void)cfg;
    return false;
}

void FsmnVad::release() {}
void FsmnVad::reset() {}

std::vector<VadSegment> FsmnVad::process(const std::vector<float>&, double) {
    return {};
}

std::vector<VadSegment> FsmnVad::flush() {
    return {};
}

void FsmnVad::updateState(float, double) {}

#endif // HAS_ONNXRUNTIME
