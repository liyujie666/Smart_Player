#include "sensevoiceengine.h"
#if HAS_ONNXRUNTIME
#include "utils/onnxruntimeutil.h"
#include "utils/fbank.h"
#endif
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <cmath>
#include <algorithm>
#include <regex>

#if HAS_ONNXRUNTIME

SenseVoiceEngine::SenseVoiceEngine() = default;
SenseVoiceEngine::~SenseVoiceEngine() { release(); }

bool SenseVoiceEngine::loadModel(const std::string& model_dir) {
    QString dir = QString::fromStdString(model_dir);

    // 查找 model_quant.onnx 或 model.onnx
    QString model_path = dir + "/model_quant.onnx";
    if (!QFileInfo::exists(model_path)) {
        model_path = dir + "/model.onnx";
        if (!QFileInfo::exists(model_path)) {
            qWarning() << "[SenseVoice] No model file found in" << dir;
            return false;
        }
    }

    impl_ = std::make_unique<Impl>();
    // 线程数控制在 2：避免抢占视频解码/渲染线程导致播放卡顿
    Ort::SessionOptions opts = OrtUtil::defaultSessionOptions(2);
    try {
        impl_->session = std::make_unique<Ort::Session>(
            OrtUtil::instance().env(), ORT_PATH(model_path), opts);
    } catch (const std::exception& e) {
        qWarning() << "[SenseVoice] failed to create session:" << e.what();
        impl_.reset();
        return false;
    }
    // memory_info 已在 Impl 构造时创建

    // 读取输入输出名称
    size_t n_inputs = impl_->session->GetInputCount();
    size_t n_outputs = impl_->session->GetOutputCount();
    Ort::AllocatorWithDefaultOptions alloc;

    for (size_t i = 0; i < n_inputs; ++i) {
        impl_->input_name_ptrs.push_back(impl_->session->GetInputNameAllocated(i, alloc));
        impl_->input_names.push_back(impl_->input_name_ptrs.back().get());
    }
    for (size_t i = 0; i < n_outputs; ++i) {
        impl_->output_name_ptrs.push_back(impl_->session->GetOutputNameAllocated(i, alloc));
        impl_->output_names.push_back(impl_->output_name_ptrs.back().get());
    }

    // 推断模型输入维度（shape[2] = FBank维度 × LFR拼接帧数）
    // SenseVoice: 560 = 80(FBank) × 7(LFR m)，FBank 维度固定 80
    int model_feat_dim = 560;
    if (n_inputs > 0) {
        auto shape = impl_->session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() >= 3 && shape[2] > 0) {
            model_feat_dim = (int)shape[2];
        }
    }
    impl_->n_mels = model_feat_dim;
    impl_->lfr_m = lfr_m;

    // FBank 维度始终是 80（不随模型输入维度变化）
    // LFR 拼接帧数 = model_feat_dim / 80
    n_mels_ = 80;
    int lfr_m = model_feat_dim / n_mels_;
    if (lfr_m <= 0 || model_feat_dim % n_mels_ != 0) {
        qWarning() << "[SenseVoice] unexpected model_feat_dim=" << model_feat_dim
                   << ", cannot derive LFR m from n_mels=" << n_mels_;
        return false;
    }

    // 记录各输入索引（SenseVoice 有 4 个输入：speech, speech_lengths, language, textnorm）
    for (size_t i = 0; i < n_inputs; ++i) {
        std::string name = impl_->input_names[i];
        if (name == "speech" || name == "speech_batch") impl_->idx_speech = (int)i;
        else if (name.find("length") != std::string::npos) impl_->idx_lengths = (int)i;
        else if (name.find("language") != std::string::npos || name.find("lang") != std::string::npos) impl_->idx_language = (int)i;
        else if (name.find("textnorm") != std::string::npos || name.find("norm") != std::string::npos) impl_->idx_textnorm = (int)i;
    }

    QStringList in_names, out_names;
    for (auto& n : impl_->input_names) in_names << n;
    for (auto& n : impl_->output_names) out_names << n;
    qDebug() << "[SenseVoice] Model loaded:" << model_path
             << "feat_dim=" << model_feat_dim
             << "fbank_mels=" << n_mels_
             << "lfr_m=" << lfr_m
             << "n_inputs=" << n_inputs
             << "n_outputs=" << n_outputs;

    return true;
}

bool SenseVoiceEngine::loadTokens(const std::string& tokens_path) {
    QFile file(QString::fromStdString(tokens_path));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[SenseVoice] Cannot open tokens.json:" << QString::fromStdString(tokens_path);
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            token_table_[i] = arr[i].toString().toStdString();
        }
    } else if (doc.isObject()) {
        QJsonObject obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            int id = it.key().toInt();
            token_table_[id] = it.value().toString().toStdString();
        }
    }

    qDebug() << "[SenseVoice] Tokens loaded:" << token_table_.size();
    return !token_table_.empty();
}

bool SenseVoiceEngine::loadConfig(const std::string& config_path) {
    QFile file(QString::fromStdString(config_path));
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = doc.object();

    // 尝试从配置中读取参数
    n_mels_ = obj.value("n_mels").toInt(80);
    frame_length_ = obj.value("frame_length").toInt(400);
    frame_shift_ = obj.value("frame_shift").toInt(160);

    return true;
}

bool SenseVoiceEngine::init(const AsrEngineConfig& cfg) {
    cfg_ = cfg;

    if (cfg_.model_path.empty()) {
        qWarning() << "[SenseVoice] model path is empty";
        return false;
    }

    QString model_dir = QString::fromStdString(cfg_.model_path);
    QDir dir(model_dir);
    if (!dir.exists()) {
        qWarning() << "[SenseVoice] model directory not found:" << model_dir;
        return false;
    }

    // 1. 加载 ONNX 模型
    if (!loadModel(cfg_.model_path)) return false;

    // 2. 加载 tokens.json
    QString tokens_path = model_dir + "/tokens.json";
    if (!loadTokens(tokens_path.toStdString())) {
        qWarning() << "[SenseVoice] Failed to load tokens.json";
        return false;
    }

    // 3. 加载 configuration.json (可选)
    QString config_path = model_dir + "/configuration.json";
    loadConfig(config_path.toStdString());

    ready_ = true;
    qDebug() << "[SenseVoice] Engine ready, model_dir:" << model_dir;
    return true;
}

void SenseVoiceEngine::release() {
#if HAS_ONNXRUNTIME
    impl_.reset();
#endif
    token_table_.clear();
    ready_ = false;
}

bool SenseVoiceEngine::recognize(const std::vector<float>& pcm,
                                  std::vector<SubtitleItem>& out,
                                  double base_sec) {
    if (!ready_ || pcm.empty()) return false;
    out.clear();

#if HAS_ONNXRUNTIME
    if (!impl_ || !impl_->session) return false;

    // 1. FBank 特征提取（80 维）
    FbankExtractor fbank;
    FbankExtractor::Config fbank_cfg;
    fbank_cfg.n_mels = n_mels_;        // 80
    fbank_cfg.frame_length = frame_length_;
    fbank_cfg.frame_shift = frame_shift_;
    fbank.init(fbank_cfg);

    auto fbank_feats = fbank.extract(pcm);
    if (fbank_feats.empty()) return false;

    const int T_raw = (int)fbank_feats.size();
    const int n_mels = n_mels_;        // 80
    const int lfr_m = impl_->lfr_m;    // 7
    const int pad = (lfr_m - 1) / 2;   // LFR 左侧上下文帧数
    const int feat_dim = n_mels * lfr_m;  // 560

    // 2. LFR(m, 1)：左侧复制首帧 padding，取 m 帧窗口拼接
    std::vector<float> flat_features((size_t)T_raw * feat_dim, 0.0f);
    for (int i = 0; i < T_raw; ++i) {
        for (int k = 0; k < lfr_m; ++k) {
            int src = i - pad + k;
            if (src < 0) src = 0;
            if (src >= T_raw) src = T_raw - 1;
            const auto& f = fbank_feats[src];
            std::copy(f.begin(), f.begin() + n_mels,
                      flat_features.begin() + (size_t)i * feat_dim + (size_t)k * n_mels);
        }
    }

    int num_frames = T_raw;

    // 3. 按输入索引构造全部输入张量
    int n_inputs = (int)impl_->input_names.size();
    std::vector<Ort::Value> inputs;
    std::vector<int64_t> lengths_data = {num_frames};
    std::vector<int64_t> lengths_shape = {1};

    // 预分配所有输入需要的张量数据（保持在生命周期内）
    std::vector<int64_t> language_data = {0};   // 0=auto
    std::vector<int64_t> textnorm_data = {15};  // 15=带标点/反归一化
    std::vector<int64_t> scalar_shape = {1};

    for (int i = 0; i < n_inputs; ++i) {
        if (i == impl_->idx_speech) {
            std::vector<int64_t> speech_shape = {1, num_frames, feat_dim};
            inputs.push_back(Ort::Value::CreateTensor<float>(
                impl_->memory_info, flat_features.data(), flat_features.size(),
                speech_shape.data(), speech_shape.size()));
        } else if (i == impl_->idx_lengths) {
            inputs.push_back(Ort::Value::CreateTensor<int64_t>(
                impl_->memory_info, lengths_data.data(), lengths_data.size(),
                lengths_shape.data(), lengths_shape.size()));
        } else if (i == impl_->idx_language) {
            inputs.push_back(Ort::Value::CreateTensor<int64_t>(
                impl_->memory_info, language_data.data(), language_data.size(),
                scalar_shape.data(), scalar_shape.size()));
        } else if (i == impl_->idx_textnorm) {
            inputs.push_back(Ort::Value::CreateTensor<int64_t>(
                impl_->memory_info, textnorm_data.data(), textnorm_data.size(),
                scalar_shape.data(), scalar_shape.size()));
        }
    }

    // 4. 推理（try-catch 防止 ONNX 异常导致 abort）
    std::vector<Ort::Value> outputs;
    try {
        outputs = impl_->session->Run(
            Ort::RunOptions{nullptr},
            impl_->input_names.data(),
            inputs.data(),
            inputs.size(),
            impl_->output_names.data(),
            impl_->output_names.size());
    } catch (const Ort::Exception& e) {
        qWarning() << "[SenseVoice] ONNX Run failed:" << e.what();
        return false;
    } catch (const std::exception& e) {
        qWarning() << "[SenseVoice] inference failed:" << e.what();
        return false;
    }

    if (outputs.empty()) return false;

    // 5. CTC 解码
    auto& logits = outputs[0];
    auto logits_shape = logits.GetTensorTypeAndShapeInfo().GetShape();

    // logits shape: (1, T, vocab_size)
    int output_T = (logits_shape.size() >= 2) ? (int)logits_shape[1] : 0;
    int vocab_size = (logits_shape.size() >= 3) ? (int)logits_shape[2] : 0;

    if (output_T == 0 || vocab_size == 0) return false;

    // 获取实际输出长度（如果有第二个输出）
    int actual_T = output_T;
    if (outputs.size() >= 2) {
        int64_t* lens_ptr = outputs[1].GetTensorMutableData<int64_t>();
        actual_T = (int)lens_ptr[0];
    }

    // Greedy CTC 解码
    float* logits_data = logits.GetTensorMutableData<float>();
    std::vector<int64_t> token_ids;
    int prev_token = -1;

    for (int t = 0; t < actual_T; ++t) {
        // 找最大概率的 token
        float max_val = -INFINITY;
        int max_id = 0;
        for (int v = 0; v < vocab_size; ++v) {
            float val = logits_data[t * vocab_size + v];
            if (val > max_val) {
                max_val = val;
                max_id = v;
            }
        }

        // CTC 去重: 连续相同的 token 只保留一个
        if (max_id != prev_token && max_id != 0) { // 0 通常是 blank token
            token_ids.push_back(max_id);
        }
        prev_token = max_id;
    }

    // 6. Token → 文本
    std::string text = ctcDecode(token_ids.data(), (int)token_ids.size());

    // 7. 后处理（去除标签）
    text = postProcess(text);

    if (text.empty()) return false;

    SubtitleItem item;
    item.text = text;
    item.start_sec = base_sec;
    item.end_sec = base_sec + (double)pcm.size() / 16000.0;
    item.language = cfg_.language;
    out.push_back(item);

    return true;
#else
    return false;
#endif
}

std::string SenseVoiceEngine::ctcDecode(const int64_t* token_ids, int length) {
    std::string text;
    for (int i = 0; i < length; ++i) {
        int id = (int)token_ids[i];
        auto it = token_table_.find(id);
        if (it != token_table_.end()) {
            text += it->second;
        }
    }
    return text;
}

std::string SenseVoiceEngine::postProcess(const std::string& text) {
    // 去除 SenseVoice 特殊标签:
    // <|zh|>, <|en|>, <|ja|>, <|ko|>, <|yue|>  (语言标签)
    // <|HAPPY|>, <|SAD|>, <|ANGRY|>, <|NEUTRAL|>, <|FEARFUL|>, <|DISGUSTED|>, <|SURPRISED|>  (情感标签)
    // <|Speech|>, <|BGM|>, <|Laughter|>, <|Applause|>  (事件标签)
    // <|woitn|>, <|withitn|> (噪声标签)

    std::string result = text;
    std::regex tag_pattern("<\\|[^|]*\\|>");
    result = std::regex_replace(result, tag_pattern, "");

    // 去除首尾空白
    size_t start = result.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = result.find_last_not_of(" \t\n\r");
    result = result.substr(start, end - start + 1);

    return result;
}

void SenseVoiceEngine::reset() {
    // 无状态引擎，无需重置
}

#else // !HAS_ONNXRUNTIME

// ========== 无 ONNX Runtime 的空实现 ==========

SenseVoiceEngine::SenseVoiceEngine() = default;
SenseVoiceEngine::~SenseVoiceEngine() { release(); }

bool SenseVoiceEngine::init(const AsrEngineConfig& cfg) {
    qWarning() << "[SenseVoice] Built without ONNX Runtime support. SenseVoice disabled.";
    (void)cfg;
    return false;
}

void SenseVoiceEngine::release() {}
void SenseVoiceEngine::reset() {}

bool SenseVoiceEngine::recognize(const std::vector<float>&,
                                  std::vector<SubtitleItem>&,
                                  double) {
    return false;
}

std::string SenseVoiceEngine::ctcDecode(const int64_t*, int) { return ""; }
std::string SenseVoiceEngine::postProcess(const std::string& text) { return text; }

#endif // HAS_ONNXRUNTIME
