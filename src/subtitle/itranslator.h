#ifndef ITRANSLATOR_H
#define ITRANSLATOR_H

#include <string>
#include <vector>
#include <memory>

struct TranslateConfig {
    std::string model_path;        // 本地模型路径（NLLB/MarianMT）
    std::string api_key;           // 云API密钥（应从环境变量读取）
    std::string api_endpoint;      // 云API地址
    std::string source_lang = "auto";
    std::string target_lang = "zh";
    int max_batch_size = 8;        // 批量翻译最大句数
    int timeout_ms = 5000;         // 请求超时(ms)
};

// 翻译结果
struct TranslateResult {
    std::string source_text;
    std::string translated_text;
    bool success = false;
    std::string error_msg;
};

class ITranslator {
public:
    virtual ~ITranslator() = default;

    virtual bool init(const TranslateConfig& cfg) = 0;
    virtual void release() = 0;
    virtual bool isReady() const = 0;

    // 单句翻译
    virtual TranslateResult translate(const std::string& text) = 0;

    // 批量翻译（提高吞吐）
    virtual std::vector<TranslateResult> translateBatch(
        const std::vector<std::string>& texts) = 0;

    virtual std::string name() const = 0;
};

// 翻译引擎类型
enum class TranslatorType {
    GPT,          // OpenAI/兼容API
    NLLB,         // Meta NLLB 本地推理
    MarianMT,     // MarianMT 本地推理
    TencentCloud// 腾讯翻译API
};

// 工厂函数
std::unique_ptr<ITranslator> createTranslator(TranslatorType type);

#endif // ITRANSLATOR_H
