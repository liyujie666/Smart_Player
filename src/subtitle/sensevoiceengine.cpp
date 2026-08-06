#include "sensevoiceengine.h"
#include <QDebug>

// SenseVoice 实现细节（pimpl 隐藏 onnxruntime 依赖）
struct SenseVoiceEngine::Impl {
    // TODO: 集成实际ONNX 推理
    //Ort::Env env;
    // Ort::Session encoder_session;
    // std::vector<float> fbank_cache;  // 特征提取缓存
    bool valid = false;
};

SenseVoiceEngine::SenseVoiceEngine() = default;
SenseVoiceEngine::~SenseVoiceEngine() { release(); }

bool SenseVoiceEngine::init(const AsrEngineConfig& cfg) {
    cfg_ = cfg;

    if (cfg_.model_path.empty()) {
        qDebug() << "[SenseVoiceEngine] model path is empty";
        return false;
    }

    impl_ = std::make_unique<Impl>();

    // TODO: 实际加载模型
    // 1. 加载 encoder.onnx
    // 2. 加载 tokenizer / vocab
    // 3. 初始化 FBank 特征提取器(80-dim, 25ms window, 10ms shift)

    qDebug() << "[SenseVoiceEngine] initialized with model:"
             << QString::fromStdString(cfg_.model_path);

    // ready_ = impl_->valid;
    ready_ = false;  // 设为 false 直到实际模型加载完成
    return ready_;
}

void SenseVoiceEngine::release() {
    impl_.reset();
    ready_ = false;
}

bool SenseVoiceEngine::recognize(const std::vector<float>& pcm,
                                  std::vector<SubtitleItem>& out,
                                  double base_sec) {
    if (!ready_ || pcm.empty()) return false;
    out.clear();

    // TODO: 实际推理流程:
    // 1. PCM → FBank 特征 (80-dim mel filterbank)
    // 2. 特征 → Encoder 推理
    // 3. CTC / Attention 解码 → token 序列
    // 4. Token → 文本 + 时间戳对齐
    //
    // SenseVoice 输出格式:<|语言|><|情感|><|事件|>文本内容
    // 需要后处理去除标签，保留纯文本

    return false;
}

void SenseVoiceEngine::reset() {
    //清除流式缓存
}
