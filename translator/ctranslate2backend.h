#ifndef CTRANSLATE2_BACKEND_H
#define CTRANSLATE2_BACKEND_H

#include "subtitletranslator.h"
#include <ctranslate2/translator.h>
#include <sentencepiece/sentencepiece_processor.h>
#include <memory>
#include <unordered_map>
#include <string>

/**
 * @brief 基于CTranslate2 + NLLB-200的翻译后端
 *
 * 性能优化策略：
 * 1. 模型量化：使用int8量化，推理速度提升2-4x，显存/内存减少50%+
 * 2. 批量翻译：支持batch，但字幕场景通常单条处理
 * 3. GPU加速：支持CUDA，CPU也可运行
 * 4. SentencePiece分词缓存
 */
class CTranslate2Backend : public ITranslateBackend
{
public:
    CTranslate2Backend();
    ~CTranslate2Backend() override;

    bool initialize(const QString &modelPath) override;
    QString translate(const QString &text, const QString &srcLang, const QString &tgtLang) override;
    void release() override;
    QString name() const override { return "CTranslate2-NLLB"; }

private:
    // 将语言代码映射为NLLB格式 (如 "en" -> "eng_Latn", "zh" -> "zho_Hans")
    std::string toNllbLangCode(const QString &lang) const;

    // 分词
    std::vector<std::string> tokenize(const std::string &text);
    // 反分词
    std::string detokenize(const std::vector<std::string> &tokens);

private:
    std::unique_ptr<ctranslate2::Translator> m_translator;
    std::unique_ptr<sentencepiece::SentencePieceProcessor> m_tokenizer;

    // 语言代码映射表
    std::unordered_map<std::string, std::string> m_langMap;

    bool m_initialized = false;
};

#endif // CTRANSLATE2_BACKEND_H
