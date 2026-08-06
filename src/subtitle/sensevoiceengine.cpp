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
    impl_->session_options = std::make_unique<Ort::SessionOptions>(
        OrtUtil::defaultSessionOptions(4));
    impl_->session = std::make_unique<Ort::Session>(
        OrtUtil::instance().env(), model_path.toUtf8().constData(), *impl_->session_options);
    impl_->memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

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

    // 推断 mel 维度
    if (n_inputs > 0) {
        auto shape = impl_->session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() >= 3 && shape[2] > 0) {
            impl_->n_mels = (int)shape[2];
            n_mels_ = impl_->n_mels;
        }
    }

    qDebug() << "[SenseVoice] Model loaded:" << model_path
             << "n_mels=" << n_mels_
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

    // 1. FBank 特征提取
    FbankExtractor fbank;
    FbankExtractor::Config fbank_cfg;
    fbank_cfg.n_mels = n_mels_;
    fbank_cfg.frame_length = frame_length_;
    fbank_cfg.frame_shift = frame_shift_;
    fbank.init(fbank_cfg);

    auto features = fbank.extract(pcm);
    if (features.empty()) return false;

    int num_frames = (int)features.size();
    int n_mels = n_mels_;

    // 2. 展平特征为 1D 数组 (1, T, n_mels)
    std::vector<float> flat_features(num_frames * n_mels);
    for (int t = 0; t < num_frames; ++t) {
        for (int m = 0; m < n_mels; ++m) {
            flat_features[t * n_mels + m] = features[t][m];
        }
    }

    // 3. 创建输入张量
    std::vector<int64_t> speech_shape = {1, num_frames, n_mels};
    Ort::Value speech_tensor = Ort::Value::CreateTensor<float>(
        impl_->memory_info, flat_features.data(), flat_features.size(),
        speech_shape.data(), speech_shape.size());

    std::vector<int64_t> lengths_data = {num_frames};
    std::vector<int64_t> lengths_shape = {1};
    Ort::Value lengths_tensor = Ort::Value::CreateTensor<int64_t>(
        impl_->memory_info, lengths_data.data(), lengths_data.size(),
        lengths_shape.data(), lengths_shape.size());

    // 4. 推理
    std::vector<Ort::Value> inputs;
    inputs.push_back(std::move(speech_tensor));
    // 如果模型需要 speech_lengths 输入
    if (impl_->input_names.size() >= 2) {
        inputs.push_back(std::move(lengths_tensor));
    }

    auto outputs = impl_->session->Run(
        Ort::RunOptions{nullptr},
        impl_->input_names.data(),
        inputs.data(),
        inputs.size(),
        impl_->output_names.data(),
        impl_->output_names.size());

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
