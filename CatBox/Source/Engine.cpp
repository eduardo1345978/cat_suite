#include "Engine.h"

namespace catbox
{
std::unique_ptr<Engine> makeAnalogEngine();
std::unique_ptr<Engine> makeAcidEngine();
std::unique_ptr<Engine> makeDrumsEngine();
std::unique_ptr<Engine> makeKeysEngine();
std::unique_ptr<Engine> makeGlideEngine();
std::unique_ptr<Engine> makeFMEngine();
std::unique_ptr<Engine> makeWavetableEngine();
std::unique_ptr<Engine> makeChoirEngine();
std::unique_ptr<Engine> makeSamplerEngine();
std::unique_ptr<Engine> makeGranularEngine();

std::unique_ptr<Engine> makeEngine (int type)
{
    switch (type)
    {
        case EngineAcid:  return makeAcidEngine();
        case EngineDrums: return makeDrumsEngine();
        case EngineKeys:  return makeKeysEngine();
        case EngineGlide: return makeGlideEngine();
        case EngineFM:    return makeFMEngine();
        case EngineWavetable: return makeWavetableEngine();
        case EngineChoir: return makeChoirEngine();
        case EngineSampler: return makeSamplerEngine();
        case EngineGranular: return makeGranularEngine();
        default:          return makeAnalogEngine();
    }
}
} // namespace catbox
