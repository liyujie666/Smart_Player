#ifndef TRANSLATOR_INTEGRATION_H
#define TRANSLATOR_INTEGRATION_H

#include "subtitletranslator.h"
#include <QObject>
#include <QString>

/**
 * @brief 翻译器与字幕系统的集成层
 *
 * 使用方式：
 * 1. 在播放器初始化时创建此对象
 * 2. 连接whisper字幕输出到本类的onSubtitleGenerated槽
 * 3. 连接本类的bilingualSubtitleReady信号到字幕渲染层
 *
 * 示例集成代码：
 *
 *   // 在播放器初始化中
 *   m_translatorIntegration = new TranslatorIntegration(this);
 *   m_translatorIntegration->initialize("models/nllb-200-distilled-600M-int8");
 *   m_translatorIntegration->setLanguagePair("en", "zh");
 *   m_translatorIntegration->setEnabled(true);
 *
 *   // 连接whisper输出
 *   connect(m_whisperEngine, &WhisperEngine::subtitleReady,
 *           m_translatorIntegration, &TranslatorIntegration::onSubtitleGenerated);
 *
 *   // 连接到字幕渲染
 *   connect(m_translatorIntegration, &TranslatorIntegration::bilingualSubtitleReady,
 *           m_subtitleRenderer, &SubtitleRenderer::showBilingualSubtitle);
 */
class TranslatorIntegration : public QObject
{
    Q_OBJECT

public:
    explicit TranslatorIntegration(QObject *parent = nullptr);
    ~TranslatorIntegration();

    /**
     * @brief 初始化翻译模型
     * @param modelPath NLLB模型目录路径
     * @return 是否成功
     */
    bool initialize(const QString &modelPath);

    /**
     * @brief 设置翻译语言对
     * @param srcLang 源语言 (如 "en", "ja")
     * @param tgtLang 目标语言 (如 "zh")
     */
    void setLanguagePair(const QString &srcLang, const QString &tgtLang);

    /**
     * @brief 启用/禁用翻译（可运行时切换）
     */
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

public slots:
    /**
     * @brief 接收whisper产生的字幕
     * @param text 识别的文本
     * @param startMs 字幕开始时间戳
     * @param endMs 字幕结束时间戳
     */
    void onSubtitleGenerated(const QString &text, int64_t startMs, int64_t endMs);

signals:
    /**
     * @brief 双语字幕就绪
     * @param original 原文
     * @param translated 译文
     * @param timestampMs 时间戳
     */
    void bilingualSubtitleReady(const QString &original, const QString &translated, int64_t timestampMs);

    /**
     * @brief 仅原文字幕（翻译禁用或失败时）
     */
    void originalSubtitleReady(const QString &text, int64_t timestampMs);

private slots:
    void onTranslationReady(const TranslateResult &result);

private:
    SubtitleTranslator *m_translator = nullptr;
    bool m_enabled = false;
    bool m_initialized = false;
};

#endif // TRANSLATOR_INTEGRATION_H
