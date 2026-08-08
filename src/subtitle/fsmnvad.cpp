#include "fsmnvad.h"
#if HAS_ONNXRUNTIME
#include "utils/onnxruntimeutil.h"
#include "utils/fbank.h"
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
    QString model_path = dir + "/model_quant.onnx";
    QString mvn_path = dir + "/am.mvn";

    // 检查文件存在
    if (!QFileInfo::exists(model_path)) {
        qWarning() << "[FsmnVad] model.onnx not found:" << model_path;
        return false;
    }

    // 创建 ONNX Runtime session
    ort_ctx_ = std::make_unique<OrtContext>();
    Ort::SessionOptions opts = OrtUtil::defaultSessionOptions(1);
    try {
        ort_ctx_->session = std::make_unique<Ort::Session>(
            OrtUtil::instance().env(), ORT_PATH(model_path), opts);
    } catch (const std::exception& e) {
        qWarning() << "[FsmnVad] failed to create session:" << e.what();
        ort_ctx_.reset();
        return false;
    }

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

    if (n_inputs == 0 || n_outputs == 0) {
        qWarning() << "[FsmnVad] invalid model io: inputs=" << n_inputs << "outputs=" << n_outputs;
        ort_ctx_.reset();
        return false;
    }

    // 输入0 = speech (1, T, feat_dim)，读取特征维度
    auto sp_shape = ort_ctx_->session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if (!sp_shape.empty() && sp_shape.back() > 0) {
        ort_ctx_->feat_dim = (int)sp_shape.back();
    } else {
        ort_ctx_->feat_dim = n_mels_ * lfr_m_;   // 80 * 5 = 400
    }

    // 输入1..N-1 = 各 cache，按模型声明的 shape 零初始化
    caches_.clear();
    ort_ctx_->cache_shapes.clear();
    for (size_t i = 1; i < n_inputs; ++i) {
        auto shape = ort_ctx_->session->GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
        size_t count = 1;
        for (auto& d : shape) {
            if (d <= 0) d = 1;   // 动态维度按 1 处理
            count *= (size_t)d;
        }
        ort_ctx_->cache_shapes.push_back(shape);
        caches_.emplace_back(count, 0.0f);
    }

    // 校验特征维度与 FBank+LFR 配置匹配性
    if (ort_ctx_->feat_dim != n_mels_ * lfr_m_) {
        if (ort_ctx_->feat_dim % n_mels_ == 0) {
            lfr_m_ = ort_ctx_->feat_dim / n_mels_;   // 自适应 LFR 拼接帧数
        } else {
            qWarning() << "[FsmnVad] unsupported feat_dim=" << ort_ctx_->feat_dim
                       << "(n_mels=" << n_mels_ << "), VAD disabled";
            ort_ctx_.reset();
            return false;
        }
    }

    QStringList in_names, out_names;
    for (auto& n : ort_ctx_->input_names) in_names << n;
    for (auto& n : ort_ctx_->output_names) out_names << n;
    qDebug() << "[FsmnVad] Model loaded: feat_dim=" << ort_ctx_->feat_dim
             << "lfr_m=" << lfr_m_
             << "caches=" << caches_.size()
             << "inputs:" << in_names
             << "outputs:" << out_names;

    // 加载 CMVN（维度须与 feat_dim 一致才生效）
    if (QFileInfo::exists(mvn_path)) {
        auto cmvn = OrtUtil::loadCmvn(mvn_path.toStdString());
        if ((int)cmvn.mean.size() == ort_ctx_->feat_dim &&
            (int)cmvn.variance.size() == ort_ctx_->feat_dim) {
            cmvn_mean_ = std::move(cmvn.mean);
            cmvn_var_ = std::move(cmvn.variance);
        } else {
            qWarning() << "[FsmnVad] CMVN dim mismatch: mean=" << cmvn.mean.size()
                       << "var=" << cmvn.variance.size()
                       << "expected=" << ort_ctx_->feat_dim << ", skip CMVN";
        }
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
    base_initialized_ = false;
    diagnostic_logged_ = false;
    for (auto& c : caches_) std::fill(c.begin(), c.end(), 0.0f);

    ready_ = true;
    qDebug() << "[FsmnVad] initialized, frame_shift_ms=" << frame_shift_ms_;
    return ready_;
}

void FsmnVad::release() {
    ort_ctx_.reset();
    caches_.clear();
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
    base_initialized_ = false;
    diagnostic_logged_ = false;
    prob_history_.clear();
    for (auto& c : caches_) std::fill(c.begin(), c.end(), 0.0f);
}

std::vector<float> FsmnVad::computeFeatures(const std::vector<float>& pcm, int& out_frames) {
    out_frames = 0;

    // 诊断：打印 PCM 输入范围
    if (!diagnostic_logged_ && !pcm.empty()) {
        float pcm_min = pcm[0], pcm_max = pcm[0], pcm_abs_max = 0.0f;
        for (float v : pcm) {
            if (v < pcm_min) pcm_min = v;
            if (v > pcm_max) pcm_max = v;
            if (std::abs(v) > pcm_abs_max) pcm_abs_max = std::abs(v);
        }
        qDebug() << "[FsmnVad] PCM input range: min=" << pcm_min 
                 << "max=" << pcm_max << "abs_max=" << pcm_abs_max;
    }

    FbankExtractor fbank;
    FbankExtractor::Config fcfg;
    fcfg.n_mels = n_mels_;
    fbank.init(fcfg);

    auto feats = fbank.extract(pcm);
    if (feats.empty()) return {};

    const int T = (int)feats.size();
    const int pad = (lfr_m_ - 1) / 2;          // LFR 左侧上下文帧数
    const int feat_dim = n_mels_ * lfr_m_;
    const int T_lfr = (lfr_n_ > 0) ? ((T + lfr_n_ - 1) / lfr_n_) : T;

    // LFR(m, n): 左侧复制首帧 padding，按步进 n 取 m 帧窗口拼接
    std::vector<float> flat((size_t)T_lfr * feat_dim, 0.0f);
    for (int i = 0; i < T_lfr; ++i) {
        const int center = i * lfr_n_;
        for (int k = 0; k < lfr_m_; ++k) {
            int src = center - pad + k;
            if (src < 0) src = 0;
            if (src >= T) src = T - 1;
            const auto& f = feats[src];
            std::copy(f.begin(), f.begin() + n_mels_,
                      flat.begin() + (size_t)i * feat_dim + (size_t)k * n_mels_);
        }
    }

    // CMVN: (x + mean) * var
    if ((int)cmvn_mean_.size() == feat_dim && (int)cmvn_var_.size() == feat_dim) {
        for (int i = 0; i < T_lfr; ++i) {
            float* row = flat.data() + (size_t)i * feat_dim;
            for (int d = 0; d < feat_dim; ++d) {
                row[d] = (row[d] + cmvn_mean_[d]) * cmvn_var_[d];
            }
        }
    }

    out_frames = T_lfr;
    return flat;
}

std::vector<float> FsmnVad::inferFeatures(const std::vector<float>& flat_feats, int num_frames) {
    if (!ort_ctx_ || !ort_ctx_->session || num_frames <= 0) return {};

    const int feat_dim = ort_ctx_->feat_dim;
    if ((int)flat_feats.size() != (size_t)num_frames * feat_dim) return {};

    // 输入0: speech (1, T, feat_dim)
    std::vector<int64_t> sp_shape = {1, num_frames, feat_dim};
    std::vector<Ort::Value> inputs;
    inputs.push_back(Ort::Value::CreateTensor<float>(
        ort_ctx_->memory_info, const_cast<float*>(flat_feats.data()), flat_feats.size(),
        sp_shape.data(), sp_shape.size()));

    // 输入1..N-1: 各 cache
    for (size_t i = 0; i < caches_.size(); ++i) {
        auto& shape = ort_ctx_->cache_shapes[i];
        inputs.push_back(Ort::Value::CreateTensor<float>(
            ort_ctx_->memory_info, caches_[i].data(), caches_[i].size(),
            shape.data(), shape.size()));
    }

    // 推理（try-catch 防止 ONNX 异常导致 abort）
    std::vector<Ort::Value> outputs;
    try {
        outputs = ort_ctx_->session->Run(
            Ort::RunOptions{nullptr},
            ort_ctx_->input_names.data(),
            inputs.data(),
            inputs.size(),
            ort_ctx_->output_names.data(),
            ort_ctx_->output_names.size());
    } catch (const Ort::Exception& e) {
        qWarning() << "[FsmnVad] ONNX Run failed:" << e.what();
        return {};
    } catch (const std::exception& e) {
        qWarning() << "[FsmnVad] inference failed:" << e.what();
        return {};
    }

    if (outputs.empty()) return {};

    // 输出1.. = 更新后的 cache，回写
    for (size_t i = 0; i + 1 < outputs.size() && i < caches_.size(); ++i) {
        try {
            const float* data = outputs[i + 1].GetTensorData<float>();
            size_t count = outputs[i + 1].GetTensorTypeAndShapeInfo().GetElementCount();
            if (count == caches_[i].size()) {
                std::copy(data, data + count, caches_[i].begin());
            }
        } catch (const std::exception&) {
            // cache 输出类型异常时保留旧 cache
        }
    }

    // 输出0是多类声学 PDF（随附模型为 248 类），配置中的 sil_pdf_ids=[0]。
    // 语音后验应为 1 - P(silence)，不能只对类别 0、1 做二分类 softmax。
    auto shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    if (shape.size() < 3 || shape[2] < 2) return {};

    const int T = (int)shape[1];
    const int C = (int)shape[2];
    const float* logits = outputs[0].GetTensorData<float>();

    std::vector<float> probs(T);
    for (int t = 0; t < T; ++t) {
        const float* row = logits + (size_t)t * C;
        float max_logit = row[0];
        for (int c = 1; c < C; ++c) max_logit = std::max(max_logit, row[c]);

        double denominator = 0.0;
        for (int c = 0; c < C; ++c) {
            denominator += std::exp((double)row[c] - max_logit);
        }
        const double silence_prob = std::exp((double)row[0] - max_logit) / denominator;
        probs[t] = std::clamp((float)(1.0 - silence_prob), 0.0f, 1.0f);
    }
    return probs;
}

std::vector<VadSegment> FsmnVad::process(const std::vector<float>& pcm, double base_sec) {
    completed_segments_.clear();
    if (!ready_ || !ort_ctx_) return {};

    // 首次调用记录流起点，用于换算帧时间
    if (!base_initialized_) {
        stream_base_sec_ = base_sec;
        base_initialized_ = true;
    }

    // 拼接上次残余的 PCM
    std::vector<float> audio = std::move(residual_pcm_);
    residual_pcm_.clear();
    audio.insert(audio.end(), pcm.begin(), pcm.end());

    const int frame_shift = 160;    // 10ms @ 16k
    const int frame_length = 400;   // 25ms @ 16k
    const int overlap = frame_length - frame_shift;
    const size_t samples_per_infer = (size_t)infer_chunk_frames_ * frame_shift;

    size_t pos = 0;
    while (pos + samples_per_infer + overlap <= audio.size()) {
        std::vector<float> seg(audio.begin() + pos,
                               audio.begin() + pos + samples_per_infer + overlap);

        int nf = 0;
        auto feats = computeFeatures(seg, nf);
        if (nf > 0) {
            auto probs = inferFeatures(feats, nf);
            if (probs.empty()) {
                qWarning() << "[FsmnVad] inference unavailable, disabling VAD";
                ready_ = false;
                return {};
            }
            for (float p : probs) {
                // 概率滑动平均平滑（减少逐帧波动导致的振荡）
                float smoothed = p;
                if (cfg_.smoothing_window > 1) {
                    prob_history_.push_back(p);
                    if ((int)prob_history_.size() > cfg_.smoothing_window) {
                        prob_history_.erase(prob_history_.begin());
                    }
                    float sum = 0;
                    for (float h : prob_history_) sum += h;
                    smoothed = sum / prob_history_.size();
                }

                double frame_time = stream_base_sec_ +
                    (double)(total_frames_processed_ * frame_shift_ms_) / 1000.0;
                updateState(smoothed, frame_time);
                // Speech 状态下每 50 帧（500ms）采样一次概率，用于诊断断句效果
                if (state_ == State::Speech && (total_frames_processed_ % 50) == 0) {
                    qDebug() << "[FsmnVad] speech prob @" << frame_time
                             << "s prob=" << smoothed;
                }
                total_frames_processed_++;
            }
            // 每个流只打印一次诊断信息，避免 10 秒块边界造成“first infer”假象
            if (!diagnostic_logged_) {
                diagnostic_logged_ = true;
                float max_p = *std::max_element(probs.begin(), probs.end());
                // 特征统计：前 5 个值、均值、方差
                float feat_min = feats[0], feat_max = feats[0], feat_sum = 0;
                for (float v : feats) {
                    if (v < feat_min) feat_min = v;
                    if (v > feat_max) feat_max = v;
                    feat_sum += v;
                }
                float feat_mean = feat_sum / feats.size();
                qDebug() << "[FsmnVad] first infer: frames=" << probs.size()
                         << "max_prob=" << max_p
                         << "feat: min=" << feat_min << "max=" << feat_max
                         << "mean=" << feat_mean
                         << "first5=" << feats[0] << feats[1] << feats[2]
                         << feats[3] << feats[4];
            }
        }
        pos += samples_per_infer;
    }

    // 保存残余（保留窗口重叠部分供下次拼接）
    if (pos < audio.size()) {
        residual_pcm_.assign(audio.begin() + pos, audio.end());
    }

    return std::move(completed_segments_);
}

std::vector<VadSegment> FsmnVad::flush() {
    completed_segments_.clear();

    if (state_ == State::Speech || state_ == State::Trailing) {
        double end_time = stream_base_sec_ +
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
    // 迟滞阈值：进入语音用高阈值，退出语音用低阈值
    bool is_speech;
    if (state_ == State::Silence) {
        // 静音状态：需要超过高阈值才算语音
        is_speech = speech_prob >= cfg_.threshold;
    } else {
        // 语音/拖尾状态：低于低阈值才算静音
        is_speech = speech_prob >= cfg_.threshold_exit;
    }

    int silence_frames_threshold = cfg_.min_silence_ms / frame_shift_ms_;
    if (silence_frames_threshold <= 0) silence_frames_threshold = 30;

    if (state_ == State::Speech && cfg_.max_speech_ms > 0 &&
        (frame_time - current_speech_start_) * 1000.0 >= cfg_.max_speech_ms) {
        VadSegment seg;
        seg.start_sec = current_speech_start_;
        seg.end_sec = frame_time;
        completed_segments_.push_back(seg);
        qDebug() << "[FsmnVad] max-length segment @" << seg.start_sec << "-"
                 << seg.end_sec << "s";
        current_speech_start_ = frame_time;
        current_speech_end_ = frame_time;
    }

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
        } else {
            // 持续语音，更新结束时间
            current_speech_end_ = frame_time;
        }
        break;

    case State::Trailing:
        if (is_speech) {
            state_ = State::Speech;
            silence_frame_count_ = 0;
        } else {
            silence_frame_count_++;
            if (silence_frame_count_ >= silence_frames_threshold) {
                double duration_ms = (current_speech_end_ - current_speech_start_) * 1000.0;
                if (duration_ms >= cfg_.min_speech_ms) {
                    VadSegment seg;
                    seg.start_sec = current_speech_start_;
                    seg.end_sec = current_speech_end_;
                    completed_segments_.push_back(seg);
                    qDebug() << "[FsmnVad] segment @" << seg.start_sec << "-"
                             << seg.end_sec << "s dur=" << duration_ms << "ms";
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

std::vector<float> FsmnVad::computeFeatures(const std::vector<float>&, int& out_frames) {
    out_frames = 0;
    return {};
}

std::vector<float> FsmnVad::inferFeatures(const std::vector<float>&, int) {
    return {};
}

void FsmnVad::updateState(float, double) {}

#endif // HAS_ONNXRUNTIME
