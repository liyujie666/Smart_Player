#include "nllbtranslator.h"
#include <QDebug>

struct NllbTranslator::Impl {
    // TODO: CTranslate2 或 ONNX Runtime 会话
    // ctranslate2::Translator translator;
    // sp::SentencePieceProcessor tokenizer;
    bool valid = false;
};

NllbTranslator::NllbTranslator() = default;
NllbTranslator::~NllbTranslator() { release(); }

bool NllbTranslator::init(const TranslateConfig& cfg) {
    cfg_ = cfg;

    if (cfg_.model_path.empty()) {
        qDebug() << "[NllbTranslator] model path is empty";
        return false;
    }

    impl_ = std::make_unique<Impl>();

    // TODO: 实际初始化
    // 1. 加载 CTranslate2 模型 (model_path/model.bin)
    // 2. 加载 SentencePiece tokenizer (model_path/tokenizer.model)
    // 3. 设置源语言/目标语言代码（NLLB 使用 flores-200 语言代码）
    //    例如: zho_Hans (简体中文), eng_Latn (英语)

    qDebug() << "[NllbTranslator] initialized (model:"
             << QString::fromStdString(cfg_.model_path) << ")";

    ready_ = false;  // 等实际模型加载后设为 true
    return ready_;
}

void NllbTranslator::release() {
    impl_.reset();
    ready_ = false;
}

TranslateResult NllbTranslator::translate(const std::string& text) {
    TranslateResult result;
    result.source_text = text;

    if (!ready_ || text.empty()) {
        result.error_msg = "not ready or empty input";
        return result;
    }

    // TODO: 实际翻译流程
    // 1. SentencePiece tokenize
    // 2. 添加语言标记token（前缀）
    // 3. CTranslate2 translate
    // 4. SentencePiece detokenize

    return result;
}

std::vector<TranslateResult> NllbTranslator::translateBatch(const std::vector<std::string>& texts) {
    std::vector<TranslateResult> results;
    // TODO: 批量翻译实现（CTranslate2 原生支持 batch）
    for (const auto& text : texts) {
        results.push_back(translate(text));
    }
    return results;
}
