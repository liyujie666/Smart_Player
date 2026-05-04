#include "asrworker.h"
#include "utils/asrutils.h"
#include <QDebug>
AsrWorker::~AsrWorker()
{
    release();
}

bool AsrWorker::init(const AsrConfig &cfg)
{
    if(ctx_){
        qDebug() << "模型已加载";
        return false;
    }

    if(cfg.model_path.empty()){
        qDebug() << "模型为空";
        return false;
    }

    cfg_ = cfg;

    auto params = whisper_context_default_params();
    ctx_ = whisper_init_from_file_with_params(cfg_.model_path.c_str(),params);

    return ctx_ != nullptr;

}

void AsrWorker::release()
{
    if(ctx_){
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

bool AsrWorker::recognize(const std::vector<float> &pcm, std::vector<SubtitleItem> &out, double base_sec)
{
    if(!ctx_ || pcm.empty()) return false;

    out.clear();

    auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = cfg_.language == "auto" ? nullptr : cfg_.language.c_str();
    params.translate = cfg_.translate;
    params.no_context = false;
    params.single_segment = false;
    params.print_progress = false;
    params.n_threads = 4;

    if(whisper_full(ctx_,params,pcm.data(),pcm.size()) != 0) return false;

    int n = whisper_full_n_segments(ctx_);
    for(int i=0;i < n;i++){
        const char* t = whisper_full_get_segment_text(ctx_,i);
        if(!t) continue;

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

void AsrWorker::reset()
{

}
