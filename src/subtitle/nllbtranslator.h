#ifndef NLLBTRANSLATOR_H
#define NLLBTRANSLATOR_H

#include "itranslator.h"
#include <memory>

// NLLB (No Language Left Behind) 本地翻译引擎
// Meta 开源的多语言翻译模型，支持 200+语言
// 使用 CTranslate2 或ONNX Runtime 推理
class NllbTranslator : public ITranslator {
public:
    NllbTranslator();
    ~NllbTranslator() override;

    bool init(const TranslateConfig& cfg) override;
    void release() override;
    bool isReady() const override { return ready_; }

    TranslateResult translate(const std::string& text) override;
    std::vector<TranslateResult> translateBatch(const std::vector<std::string>& texts) override;

    std::string name() const override { return "NLLB"; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    TranslateConfig cfg_;
    bool ready_ = false;
};

#endif // NLLBTRANSLATOR_H
