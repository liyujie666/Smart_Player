#include "itranslator.h"
#include "gpttranslator.h"
#include "nllbtranslator.h"
#include "marianmttranslator.h"
#include "tencenttranslator.h"

std::unique_ptr<ITranslator> createTranslator(TranslatorType type)
{
    switch (type) {
    case TranslatorType::GPT:
        return std::make_unique<GptTranslator>();
    case TranslatorType::NLLB:
        return std::make_unique<NllbTranslator>();
    case TranslatorType::MarianMT:
        return std::make_unique<MarianMtTranslator>();
    case TranslatorType::TencentCloud:
        return std::make_unique<TencentTranslator>();
    }
    return nullptr;
}
