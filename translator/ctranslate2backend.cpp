#include "ctranslate2backend.h"
#include <QDebug>
#include <QDir>

CTranslate2Backend::CTranslate2Backend()
{
    // NLLB-200 语言代码映射
    m_langMap = {
        {"en", "eng_Latn"}, {"zh", "zho_Hans"}, {"ja", "jpn_Jpan"},
        {"ko", "kor_Hang"}, {"fr", "fra_Latn"}, {"de", "deu_Latn"},
        {"es", "spa_Latn"}, {"ru", "rus_Cyrl"}, {"ar", "arb_Arab"},
        {"pt", "por_Latn"}, {"it", "ita_Latn"}, {"vi", "vie_Latn"},
        {"th", "tha_Thai"}, {"id", "ind_Latn"}, {"tr", "tur_Latn"},
    };
}

CTranslate2Backend::~CTranslate2Backend()
{
    release();
}

bool CTranslate2Backend::initialize(const QString &modelPath)
{
    try {
        // 检查模型文件
        QDir modelDir(modelPath);
        if (!modelDir.exists("model.bin")) {
            qWarning() << "[CTranslate2] Model not found at:" << modelPath;
            return false;
        }

        // 初始化SentencePiece分词器
        QString spModelPath = modelDir.filePath("sentencepiece.bpe.model");
        m_tokenizer = std::make_unique<sentencepiece::SentencePieceProcessor>();
        auto status = m_tokenizer->Load(spModelPath.toStdString());
        if (!status.ok()) {
            qWarning() << "[CTranslate2] Failed to load tokenizer:" << spModelPath;
            return false;
        }

        // 初始化CTranslate2翻译模型
        // 使用int8量化 + CPU推理（低资源消耗）
        // 如有GPU可改为 ctranslate2::Device::CUDA
        m_translator = std::make_unique<ctranslate2::Translator>(
            modelPath.toStdString(),
            ctranslate2::Device::CUDA,  // 或 Device::CUDA
            ctranslate2::ComputeType::INT8  // int8量化，速度快内存小
        );

        m_initialized = true;
        qDebug() << "[CTranslate2] Model loaded from:" << modelPath;
        return true;

    } catch (const std::exception &e) {
        qWarning() << "[CTranslate2] Init failed:" << e.what();
        return false;
    }
}

QString CTranslate2Backend::translate(const QString &text, const QString &srcLang, const QString &tgtLang)
{
    if (!m_initialized || !m_translator || !m_tokenizer) {
        return QString();
    }

    try {
        std::string srcLangCode = toNllbLangCode(srcLang);
        std::string tgtLangCode = toNllbLangCode(tgtLang);

        // 分词
        std::vector<std::string> tokens = tokenize(text.toStdString());

        // NLLB格式：在开头添加源语言标记
        tokens.insert(tokens.begin(), srcLangCode);

        // 设置翻译选项
        ctranslate2::TranslationOptions options;
        options.beam_size = 4;          // beam search宽度，4是精度和速度的平衡
        options.max_decoding_length = 256;
        options.repetition_penalty = 1.2f;

        // 设置目标语言前缀
        std::vector<std::string> targetPrefix = {tgtLangCode};

        // 执行翻译
        std::vector<std::vector<std::string>> batch = {tokens};
        std::vector<std::vector<std::string>> targetPrefixes = {targetPrefix};

        auto results = m_translator->translate_batch(batch, targetPrefixes, options);

        if (results.empty() || results[0].output().empty()) {
            return QString();
        }

        // 反分词，跳过目标语言标记
        std::vector<std::string> outputTokens = results[0].output();
        if (!outputTokens.empty() && outputTokens[0] == tgtLangCode) {
            outputTokens.erase(outputTokens.begin());
        }

        std::string translated = detokenize(outputTokens);
        return QString::fromStdString(translated);

    } catch (const std::exception &e) {
        qWarning() << "[CTranslate2] Translation error:" << e.what();
        return QString();
    }
}

void CTranslate2Backend::release()
{
    m_translator.reset();
    m_tokenizer.reset();
    m_initialized = false;
}

std::string CTranslate2Backend::toNllbLangCode(const QString &lang) const
{
    std::string key = lang.toLower().toStdString();
    auto it = m_langMap.find(key);
    if (it != m_langMap.end()) {
        return it->second;
    }
    // 默认返回英语
    return "eng_Latn";
}

std::vector<std::string> CTranslate2Backend::tokenize(const std::string &text)
{
    std::vector<std::string> pieces;
    m_tokenizer->Encode(text, &pieces);
    return pieces;
}

std::string CTranslate2Backend::detokenize(const std::vector<std::string> &tokens)
{
    std::string result;
    m_tokenizer->Decode(tokens, &result);
    return result;
}
