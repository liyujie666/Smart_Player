#ifndef MARIANMTTRANSLATOR_H
#define MARIANMTTRANSLATOR_H

#include "itranslator.h"
#include <memory>

// MarianMT 本地翻译引擎
// 基于 Helsinki-NLP/OPUS 预训练模型
// 使用 CTranslate2 推理，针对特定语言对（如 en→zh）性能优秀
class MarianMtTranslator : public ITranslator {
public:
    MarianMtTranslator();
    ~MarianMtTranslator() override;

    bool init(const TranslateConfig& cfg) override;
    void release() override;
    bool isReady() const override { return ready_; }

    TranslateResult translate(const std::string& text) override;
    std::vector<TranslateResult> translateBatch(const std::vector<std::string>& texts) override;

    std::string name() const override { return "MarianMT"; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    TranslateConfig cfg_;
    bool ready_ = false;
};

#endif // MARIANMTTRANSLATOR_H
