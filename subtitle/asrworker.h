#ifndef ASRWORKER_H
#define ASRWORKER_H

#include <vector>
#include <string>
#include "whisper/whisper.h"
#include "queue/subtitlequeue.h"

struct AsrConfig{
    std::string model_path;
    std::string language = "auto";
    bool translate = false;
};

class AsrWorker{
public:
    AsrWorker() = default;
    ~AsrWorker();

    bool init(const AsrConfig& cfg);
    void release();
    bool recognize(const std::vector<float>& pcm, std::vector<SubtitleItem>& out, double base_sec = 0);
    void reset();

private:
    whisper_context* ctx_ = nullptr;
    AsrConfig cfg_;
};

#endif
