#include "translatorintegration.h"
#include <QDebug>

TranslatorIntegration::TranslatorIntegration(QObject *parent)
    : QObject(parent)
    , m_translator(new SubtitleTranslator(this))
{
    connect(m_translator, &SubtitleTranslator::translationReady,
            this, &TranslatorIntegration::onTranslationReady);

    connect(m_translator, &SubtitleTranslator::errorOccurred,
            this, [](const QString &err) {
                qWarning() << "[TranslatorIntegration]" << err;
            });
}

TranslatorIntegration::~TranslatorIntegration()
{
    if (m_translator) {
        m_translator->stop();
    }
}

bool TranslatorIntegration::initialize(const QString &modelPath)
{
    if (!m_translator->init(modelPath, "ctranslate2")) {
        return false;
    }
    m_initialized = true;
    return true;
}

void TranslatorIntegration::setLanguagePair(const QString &srcLang, const QString &tgtLang)
{
    m_translator->setLanguagePair(srcLang, tgtLang);
}

void TranslatorIntegration::setEnabled(bool enabled)
{
    m_enabled = enabled;

    if (enabled && m_initialized) {
        m_translator->start();
        qDebug() << "[TranslatorIntegration] Translation enabled";
    } else {
        m_translator->stop();
        qDebug() << "[TranslatorIntegration] Translation disabled";
    }
}

void TranslatorIntegration::onSubtitleGenerated(const QString &text, int64_t startMs, int64_t endMs)
{
    Q_UNUSED(endMs);

    if (!m_enabled || text.trimmed().isEmpty()) {
        // 翻译禁用时直接透传原文
        emit originalSubtitleReady(text, startMs);
        return;
    }

    // 提交异步翻译
    m_translator->submitTranslation(text, startMs);

    // 同时先发送原文（用户立即看到原文，翻译稍后到达）
    emit originalSubtitleReady(text, startMs);
}

void TranslatorIntegration::onTranslationReady(const TranslateResult &result)
{
    if (result.success) {
        emit bilingualSubtitleReady(result.sourceText, result.translatedText, result.timestamp);
    }
}
