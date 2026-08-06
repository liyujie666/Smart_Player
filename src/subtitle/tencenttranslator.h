#ifndef TENCENTTRANSLATOR_H
#define TENCENTTRANSLATOR_H

#include "itranslator.h"
#include <QNetworkAccessManager>
#include <memory>

// 腾讯翻译 API 引擎
// 使用腾讯云机器翻译 TMT (TextTranslateBatch) 接口
class TencentTranslator : public ITranslator {
public:
    TencentTranslator();
    ~TencentTranslator() override;

    bool init(const TranslateConfig& cfg) override;
    void release() override;
    bool isReady() const override { return ready_; }

    TranslateResult translate(const std::string& text) override;
    std::vector<TranslateResult> translateBatch(const std::vector<std::string>& texts) override;

    std::string name() const override { return "TencentCloud"; }

private:
    std::string callApi(const std::vector<std::string>& texts);
    std::string generateSignature(const std::string& payload, const std::string& timestamp) const;

private:
    TranslateConfig cfg_;
    bool ready_ = false;
    std::unique_ptr<QNetworkAccessManager> network_;
    std::string secret_id_;
    std::string secret_key_;
};

#endif // TENCENTTRANSLATOR_H
