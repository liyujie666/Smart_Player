#include "iasrengine.h"
#include "whisperengine.h"
#include "sensevoiceengine.h"
#include "cloudasrengine.h"

std::unique_ptr<IAsrEngine> createAsrEngine(AsrEngineType type)
{
    switch (type) {
    case AsrEngineType::Whisper:
        return std::make_unique<WhisperEngine>();
    case AsrEngineType::SenseVoice:
        return std::make_unique<SenseVoiceEngine>();
    case AsrEngineType::CloudASR:
        return std::make_unique<CloudAsrEngine>();
    }
    return nullptr;
}
