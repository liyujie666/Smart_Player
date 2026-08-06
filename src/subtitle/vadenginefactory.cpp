#include "ivadengine.h"
#include "fsmnvad.h"

std::unique_ptr<IVadEngine> createVadEngine(VadEngineType type)
{
    switch (type) {
    case VadEngineType::FSMN:
        return std::make_unique<FsmnVad>();
    case VadEngineType::Silero:
        // TODO: return std::make_unique<SileroVad>();
        return nullptr;
    }
    return nullptr;
}
