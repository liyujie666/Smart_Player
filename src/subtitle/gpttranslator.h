#ifndef GPTTRANSLATOR_H
#define GPTTRANSLATOR_H

#include "itranslator.h"
#include <QNetworkAccessManager>
#include <memory>

// GPT/OpenAI 兼容API翻译引擎
// 支持任何 OpenAI API兼容的端点（官方、Azure、本地 ollama 等）
class GptTranslator : public ITranslator {
public:
    GptTranslator();
    ~GptTranslator() override;

    bool init(const TranslateConfig& cfg) override;
    void release() override;
    bool isReady() const override { return ready_; }

    TranslateResult translate(const std::string& text) override;
    std::vector<TranslateResult> translateBatch(const std::vector<std::string>& texts) override;

    std::string name() const override { return "GPT"; }

private:
    std::string buildPrompt(const std::string& text) const;
    std::string buildBatchPrompt(const std::vector<std::string>& texts) const;
    std::string callApi(const std::string& prompt);

private:
    TranslateConfig cfg_;
    bool ready_ = false;
    std::unique_ptr<QNetworkAccessManager> network_;
};

#endif // GPTTRANSLATOR_H
