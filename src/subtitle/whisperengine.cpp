#include "whisperengine.h"
#include "utils/asrutils.h"
#include <QDebug>

WhisperEngine::WhisperEngine() = default;

WhisperEngine::~WhisperEngine()
{
    release();
}

bool WhisperEngine::initWithContext(whisper_context* external_ctx, const AsrEngineConfig& cfg)
{
    if (!external_ctx) return false;
    if (ctx_ && owns_context_) {
        whisper_free(ctx_);
    }
    cfg_ = cfg;
    ctx_ = external_ctx;
    owns_context_ = false;
    return true;
}

bool WhisperEngine::init(const AsrEngineConfig& cfg)
{
    if (cfg.model_path.empty()) {
        qDebug() << "[WhisperEngine] model path is empty";
        return false;
    }

    cfg_ = cfg;

    auto params = whisper_context_default_params();
    ctx_ = whisper_init_from_file_with_params(cfg_.model_path.c_str(), params);
    if (!ctx_) return false;

    owns_context_ = true;
    return true;
}

void WhisperEngine::release()
{
    if (ctx_) {
        if (owns_context_) {
            whisper_free(ctx_);
        }
        ctx_ = nullptr;
        owns_context_ = false;
    }
}

bool WhisperEngine::recognize(const std::vector<float>& pcm, std::vector<SubtitleItem>& out, double base_sec)
{
    if (!ctx_ || pcm.empty()) return false;

    out.clear();

    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = cfg_.language == "auto" ? nullptr : cfg_.language.c_str();
    params.translate = false;
    params.no_context = false;
    params.single_segment = false;
    params.print_progress = false;
    params.n_threads = cfg_.n_threads;

    if (whisper_full(ctx_, params, pcm.data(), pcm.size()) != 0) return false;

    int n = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n; ++i) {
        const char* t = whisper_full_get_segment_text(ctx_, i);
        if (!t) continue;

        std::string text = AsrUtil::normalizeText(t);
        if (text.empty()) continue;

        SubtitleItem item;
        item.start_sec = base_sec + AsrUtil::whisperTsToSec(whisper_full_get_segment_t0(ctx_, i));
        item.end_sec = base_sec + AsrUtil::whisperTsToSec(whisper_full_get_segment_t1(ctx_, i));
        item.text = text;
        out.push_back(item);
    }

    return !out.empty();
}

void WhisperEngine::reset()
{
    // whisper_full的 no_context 参数在 recognize 中处理上下文隔离
}
