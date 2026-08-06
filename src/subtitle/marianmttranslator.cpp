#include "marianmttranslator.h"
#include <QDebug>

struct MarianMtTranslator::Impl {
    // TODO: CTranslate2 translator + SentencePiece tokenizer
    bool valid = false;
};

MarianMtTranslator::MarianMtTranslator() = default;
MarianMtTranslator::~MarianMtTranslator() { release(); }

bool MarianMtTranslator::init(const TranslateConfig& cfg) {
    cfg_ = cfg;

    if (cfg_.model_path.empty()) {
        qDebug() << "[MarianMtTranslator] model path is empty";
        return false;
    }

    impl_ = std::make_unique<Impl>();

    // TODO: 加载 MarianMT CTranslate2 转换后的模型
    // MarianMT 模型通常是单向的(src→tgt)
    // 需要选择正确的语言对模型，例如:
    //   opus-mt-en-zh (英→中)
    //   opus-mt-zh-en (中→英)

    qDebug() << "[MarianMtTranslator] initialized (model:"
             << QString::fromStdString(cfg_.model_path) << ")";

    ready_ = false;
    return ready_;
}

void MarianMtTranslator::release() {
    impl_.reset();
    ready_ = false;
}

TranslateResult MarianMtTranslator::translate(const std::string& text) {
    TranslateResult result;
    result.source_text = text;

    if (!ready_ || text.empty()) {
        result.error_msg = "not ready or empty input";
        return result;
    }

    // TODO: tokenize → translate → detokenize
    return result;
}

std::vector<TranslateResult> MarianMtTranslator::translateBatch(const std::vector<std::string>& texts) {
    std::vector<TranslateResult> results;
    for (const auto& text : texts) {
        results.push_back(translate(text));
    }
    return results;
}
