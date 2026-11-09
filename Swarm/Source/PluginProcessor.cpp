#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"

using namespace juce;

namespace
{
AudioParameterFloatAttributes percent()
{
    return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
}
std::unique_ptr<AudioParameterFloat> unit (const char* id, const char* name, float def, AudioParameterFloatAttributes a = percent())
{
    return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def, a);
}
}

AudioProcessorValueTreeState::ParameterLayout SwarmProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (unit ("x", "Nebula X (dream/dark - pulse/wild)", 0.3f));
    l.add (unit ("y", "Nebula Y (dream/pulse - dark/wild)", 0.3f));
    l.add (unit ("grain", "Grain", 0.45f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { const float ms = swarm::grainMs (v); return ms < 100.0f ? String (roundToInt (ms)) + " ms" : String (ms / 1000.0f, 2) + " s"; })));
    l.add (unit ("density", "Density", 0.55f));
    l.add (unit ("pitch", "Pitch", 0.5f));
    l.add (unit ("scatter", "Scatter", 0.4f));
    l.add (unit ("life", "Life", 0.6f));
    l.add (unit ("space", "Space", 0.45f));
    l.add (unit ("tone", "Tone", 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { return v < 0.45f ? "dark " + String (roundToInt ((0.5f - v) * 200.0f)) : v > 0.55f ? "bright " + String (roundToInt ((v - 0.5f) * 200.0f)) : String ("neutral"); })));
    l.add (unit ("dirt", "Dirt", 0.1f));
    l.add (unit ("mix", "Mix", 0.6f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "sync", 1 }, "Grid", swarm::syncNames(), 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Notes", swarm::scaleNames(), 0));
    // MORE
    auto rel = [] (float v, int) { const int d = roundToInt ((v - 0.5f) * 200.0f); return d == 0 ? String ("auto") : (d > 0 ? "+" : "") + String (d) + "%"; };
    l.add (unit ("shape", "Shape (soft - chopped)", 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { return v < 0.45f ? "soft " + String (roundToInt ((0.5f - v) * 200.0f)) : v > 0.55f ? "chop " + String (roundToInt ((v - 0.5f) * 200.0f)) : String ("auto"); })));
    l.add (unit ("reverse", "Reverse", 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { return v < 0.01f ? String ("never") : v > 0.99f ? String ("always") : std::abs (v - 0.5f) < 0.03f ? String ("auto") : String (roundToInt (v * 100.0f)) + "%"; })));
    l.add (unit ("updown", "Up / down", 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { return v < 0.45f ? "down " + String (roundToInt ((0.5f - v) * 200.0f)) : v > 0.55f ? "up " + String (roundToInt ((v - 0.5f) * 200.0f)) : String ("both"); })));
    l.add (unit ("detune", "Detune", 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String::fromUTF8 ("\xc2\xb1") + String (roundToInt (v * 50.0f)) + " ct"; })));
    l.add (unit ("width", "Width", 0.67f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 150.0f)) + "%"; })));
    l.add (unit ("reach", "Reach (how far back)", 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction (rel)));
    l.add (unit ("lifespeed", "Life speed", 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { return "x" + String (std::pow (4.0f, v * 2.0f - 1.0f), 2); })));
    l.add (unit ("echo", "Echo", 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { return v < 0.01f ? String ("off") : std::abs (v - 0.5f) < 0.03f ? String ("auto") : "x" + String (v < 0.5f ? v * 2.0f : 1.0f + (v - 0.5f) * 6.0f, 1); })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "echotime", 1 }, "Echo time", swarm::echoNames(), 2));
    l.add (unit ("size", "Reverb size", 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
               { return "x" + String (0.3f + 1.4f * v, 1); })));
    l.add (unit ("duck", "Duck", 0.0f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "split", 1 }, "Dry lows below", swarm::splitNames(), 2));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

const std::vector<SwarmProcessor::Preset>& SwarmProcessor::presets()
{
    static const std::vector<Preset> list {
        { "Night Choir",      { { "x", 0.15f }, { "y", 0.3f }, { "grain", 0.62f }, { "density", 0.6f }, { "pitch", 0.55f }, { "scatter", 0.35f }, { "life", 0.55f }, { "space", 0.65f }, { "tone", 0.45f }, { "dirt", 0.0f }, { "mix", 0.65f }, { "scale", 2 } } },
        { "Glass Rain",       { { "x", 0.55f }, { "y", 0.15f }, { "grain", 0.2f }, { "density", 0.7f }, { "pitch", 0.7f }, { "scatter", 0.6f }, { "life", 0.6f }, { "space", 0.55f }, { "tone", 0.7f }, { "dirt", 0.0f }, { "mix", 0.55f }, { "scale", 3 } } },
        { "Moth Dust",        { { "x", 0.35f }, { "y", 0.6f }, { "grain", 0.35f }, { "density", 0.75f }, { "pitch", 0.3f }, { "scatter", 0.7f }, { "life", 0.7f }, { "space", 0.4f }, { "tone", 0.35f }, { "dirt", 0.2f }, { "mix", 0.6f } } },
        { "Vocal Halo",       { { "x", 0.1f }, { "y", 0.1f }, { "grain", 0.55f }, { "density", 0.55f }, { "pitch", 0.45f }, { "scatter", 0.25f }, { "life", 0.35f }, { "space", 0.6f }, { "tone", 0.6f }, { "dirt", 0.0f }, { "mix", 0.4f } } },
        { "Pad Galaxy",       { { "x", 0.2f }, { "y", 0.35f }, { "grain", 0.75f }, { "density", 0.65f }, { "pitch", 0.6f }, { "scatter", 0.5f }, { "life", 0.65f }, { "space", 0.8f }, { "tone", 0.5f }, { "dirt", 0.05f }, { "mix", 0.75f }, { "scale", 5 } } },
        { "Lead Sparkles",    { { "x", 0.45f }, { "y", 0.1f }, { "grain", 0.25f }, { "density", 0.6f }, { "pitch", 0.65f }, { "scatter", 0.45f }, { "life", 0.5f }, { "space", 0.45f }, { "tone", 0.65f }, { "dirt", 0.0f }, { "mix", 0.45f }, { "scale", 1 } } },
        { "Trap Shards",      { { "x", 0.9f }, { "y", 0.25f }, { "grain", 0.3f }, { "density", 0.7f }, { "pitch", 0.4f }, { "scatter", 0.2f }, { "life", 0.35f }, { "space", 0.25f }, { "tone", 0.5f }, { "dirt", 0.2f }, { "mix", 0.55f }, { "sync", 1 } } },
        { "Stutter Engine",   { { "x", 1.0f }, { "y", 0.0f }, { "grain", 0.35f }, { "density", 0.85f }, { "pitch", 0.2f }, { "scatter", 0.1f }, { "life", 0.25f }, { "space", 0.15f }, { "tone", 0.55f }, { "dirt", 0.15f }, { "mix", 0.8f }, { "sync", 1 } } },
        { "Triplet Fireflies",{ { "x", 0.8f }, { "y", 0.15f }, { "grain", 0.22f }, { "density", 0.6f }, { "pitch", 0.65f }, { "scatter", 0.3f }, { "life", 0.45f }, { "space", 0.45f }, { "tone", 0.65f }, { "dirt", 0.0f }, { "mix", 0.5f }, { "sync", 4 }, { "scale", 3 } } },
        { "Dark Matter",      { { "x", 0.05f }, { "y", 0.95f }, { "grain", 0.7f }, { "density", 0.55f }, { "pitch", 0.6f }, { "scatter", 0.45f }, { "life", 0.55f }, { "space", 0.6f }, { "tone", 0.3f }, { "dirt", 0.25f }, { "mix", 0.7f } } },
        { "Haunted Hallway",  { { "x", 0.3f }, { "y", 0.85f }, { "grain", 0.6f }, { "density", 0.45f }, { "pitch", 0.55f }, { "scatter", 0.65f }, { "life", 0.75f }, { "space", 0.85f }, { "tone", 0.35f }, { "dirt", 0.1f }, { "mix", 0.7f }, { "scale", 6 } } },
        { "Insect Choir",     { { "x", 0.75f }, { "y", 0.75f }, { "grain", 0.15f }, { "density", 0.9f }, { "pitch", 0.6f }, { "scatter", 0.8f }, { "life", 0.8f }, { "space", 0.35f }, { "tone", 0.6f }, { "dirt", 0.15f }, { "mix", 0.6f }, { "scale", 4 } } },
        { "Supernova",        { { "x", 0.95f }, { "y", 0.95f }, { "grain", 0.45f }, { "density", 0.85f }, { "pitch", 0.85f }, { "scatter", 0.9f }, { "life", 1.0f }, { "space", 0.7f }, { "tone", 0.55f }, { "dirt", 0.35f }, { "mix", 0.8f } } },
        { "Lo-fi Stardust",   { { "x", 0.4f }, { "y", 0.55f }, { "grain", 0.4f }, { "density", 0.5f }, { "pitch", 0.25f }, { "scatter", 0.4f }, { "life", 0.4f }, { "space", 0.4f }, { "tone", 0.3f }, { "dirt", 0.35f }, { "mix", 0.5f } } },
        // vocals
        { "Angel Shimmer",    { { "x", 0.05f }, { "y", 0.05f }, { "grain", 0.6f }, { "density", 0.6f }, { "pitch", 0.6f }, { "updown", 0.85f }, { "scatter", 0.3f }, { "life", 0.4f }, { "space", 0.7f }, { "size", 0.7f }, { "tone", 0.65f }, { "mix", 0.45f }, { "duck", 0.4f }, { "detune", 0.25f } } },
        { "Whisper Cloud",    { { "x", 0.3f }, { "y", 0.45f }, { "grain", 0.3f }, { "density", 0.8f }, { "pitch", 0.15f }, { "scatter", 0.75f }, { "reverse", 0.8f }, { "life", 0.55f }, { "space", 0.5f }, { "tone", 0.6f }, { "dirt", 0.05f }, { "mix", 0.5f }, { "width", 1.0f } } },
        { "Vocal Chop Grid",  { { "x", 1.0f }, { "y", 0.1f }, { "grain", 0.35f }, { "density", 0.75f }, { "pitch", 0.45f }, { "scale", 2 }, { "scatter", 0.15f }, { "life", 0.3f }, { "space", 0.25f }, { "shape", 0.75f }, { "mix", 0.7f }, { "sync", 1 } } },
        { "Ghost Backing",    { { "x", 0.2f }, { "y", 0.65f }, { "grain", 0.65f }, { "density", 0.5f }, { "pitch", 0.5f }, { "updown", 0.2f }, { "scatter", 0.5f }, { "life", 0.6f }, { "space", 0.75f }, { "tone", 0.35f }, { "mix", 0.4f }, { "duck", 0.6f }, { "reach", 0.8f } } },
        // pads and chords
        { "Endless Pad",      { { "x", 0.15f }, { "y", 0.3f }, { "grain", 0.9f }, { "density", 0.7f }, { "pitch", 0.35f }, { "scatter", 0.6f }, { "life", 0.5f }, { "lifespeed", 0.25f }, { "space", 0.85f }, { "size", 0.85f }, { "tone", 0.5f }, { "mix", 0.85f }, { "width", 1.0f } } },
        { "Frozen Choir",     { { "x", 0.1f }, { "y", 0.4f }, { "grain", 0.75f }, { "density", 0.65f }, { "pitch", 0.55f }, { "scale", 5 }, { "scatter", 0.45f }, { "life", 0.45f }, { "space", 0.7f }, { "tone", 0.55f }, { "mix", 0.75f }, { "detune", 0.3f } } },
        { "Tape Ocean",       { { "x", 0.35f }, { "y", 0.7f }, { "grain", 0.8f }, { "density", 0.55f }, { "pitch", 0.2f }, { "scatter", 0.55f }, { "life", 0.6f }, { "lifespeed", 0.3f }, { "space", 0.6f }, { "tone", 0.35f }, { "dirt", 0.3f }, { "mix", 0.7f }, { "detune", 0.5f } } },
        // leads and melodies
        { "Octave Comet",     { { "x", 0.4f }, { "y", 0.2f }, { "grain", 0.5f }, { "density", 0.55f }, { "pitch", 0.75f }, { "updown", 1.0f }, { "scatter", 0.35f }, { "life", 0.45f }, { "space", 0.5f }, { "echo", 0.75f }, { "tone", 0.6f }, { "mix", 0.45f } } },
        { "Glitter Arp",      { { "x", 0.85f }, { "y", 0.05f }, { "grain", 0.25f }, { "density", 0.7f }, { "pitch", 0.85f }, { "scale", 3 }, { "updown", 0.75f }, { "scatter", 0.25f }, { "life", 0.4f }, { "space", 0.4f }, { "echotime", 1 }, { "echo", 0.7f }, { "tone", 0.7f }, { "mix", 0.55f }, { "sync", 0 } } },
        { "Mirror Lead",      { { "x", 0.45f }, { "y", 0.35f }, { "grain", 0.55f }, { "density", 0.5f }, { "pitch", 0.4f }, { "reverse", 1.0f }, { "scatter", 0.3f }, { "life", 0.35f }, { "space", 0.45f }, { "tone", 0.55f }, { "mix", 0.5f } } },
        // drums and loops
        { "Drum Shatter",     { { "x", 0.95f }, { "y", 0.55f }, { "grain", 0.2f }, { "density", 0.8f }, { "pitch", 0.35f }, { "scatter", 0.4f }, { "life", 0.5f }, { "space", 0.25f }, { "shape", 0.85f }, { "dirt", 0.3f }, { "mix", 0.6f }, { "sync", 0 } } },
        { "Breakbeat Mincer", { { "x", 1.0f }, { "y", 0.3f }, { "grain", 0.3f }, { "density", 0.9f }, { "pitch", 0.25f }, { "reverse", 0.7f }, { "scatter", 0.2f }, { "life", 0.4f }, { "space", 0.15f }, { "shape", 0.8f }, { "mix", 0.75f }, { "sync", 1 } } },
        { "Half-time Haze",   { { "x", 0.6f }, { "y", 0.6f }, { "grain", 0.5f }, { "density", 0.5f }, { "pitch", 0.3f }, { "updown", 0.0f }, { "scatter", 0.35f }, { "life", 0.45f }, { "space", 0.5f }, { "echotime", 3 }, { "echo", 0.65f }, { "tone", 0.4f }, { "mix", 0.5f }, { "sync", 3 } } },
        // whole mix and fx
        { "Mix Glue Dust",    { { "x", 0.4f }, { "y", 0.4f }, { "grain", 0.55f }, { "density", 0.55f }, { "pitch", 0.1f }, { "scatter", 0.3f }, { "life", 0.3f }, { "space", 0.35f }, { "tone", 0.5f }, { "dirt", 0.1f }, { "mix", 0.2f }, { "duck", 0.3f } } },
        { "Riser Bloom",      { { "x", 0.25f }, { "y", 0.0f }, { "grain", 0.7f }, { "density", 0.85f }, { "pitch", 0.9f }, { "updown", 1.0f }, { "scatter", 0.7f }, { "life", 0.8f }, { "lifespeed", 0.8f }, { "space", 0.9f }, { "tone", 0.7f }, { "mix", 0.8f } } },
        { "Black Hole",       { { "x", 0.0f }, { "y", 1.0f }, { "grain", 1.0f }, { "density", 0.6f }, { "pitch", 0.7f }, { "updown", 0.0f }, { "scatter", 0.8f }, { "reach", 1.0f }, { "life", 0.7f }, { "lifespeed", 0.2f }, { "space", 1.0f }, { "size", 1.0f }, { "tone", 0.2f }, { "dirt", 0.3f }, { "mix", 0.85f } } },
        { "Radio From Mars",  { { "x", 0.7f }, { "y", 0.85f }, { "grain", 0.35f }, { "density", 0.65f }, { "pitch", 0.55f }, { "scale", 6 }, { "scatter", 0.6f }, { "life", 0.85f }, { "lifespeed", 0.75f }, { "space", 0.4f }, { "tone", 0.7f }, { "dirt", 0.55f }, { "mix", 0.65f }, { "detune", 0.6f } } },
        { "Comet Tail",       { { "x", 0.25f }, { "y", 0.2f }, { "grain", 0.85f }, { "density", 0.5f }, { "pitch", 0.35f }, { "scatter", 0.55f }, { "life", 0.6f }, { "space", 0.95f }, { "tone", 0.55f }, { "dirt", 0.0f }, { "mix", 0.7f } } },
    };
    return list;
}

void SwarmProcessor::set (const String& id, float v)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void SwarmProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output" && rp->getParameterID() != "freeze") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) index].values) set (id, v);
}

void SwarmProcessor::randomise()
{
    auto r = [this] (float lo, float hi) { return lo + (hi - lo) * dice.nextFloat(); };
    set ("x", r (0.0f, 1.0f)); set ("y", r (0.0f, 1.0f));
    set ("grain", r (0.15f, 0.85f)); set ("density", r (0.3f, 0.85f)); set ("pitch", r (0.15f, 0.85f));
    set ("scatter", r (0.1f, 0.85f)); set ("life", r (0.25f, 0.9f)); set ("space", r (0.2f, 0.85f));
    set ("tone", r (0.3f, 0.7f)); set ("dirt", dice.nextFloat() < 0.5f ? 0.0f : r (0.05f, 0.4f));
    // the finer ones: mostly left on auto, sometimes pushed
    auto maybe = [this, &r] (const char* id, float lo, float hi, float def) { set (id, dice.nextFloat() < 0.35f ? r (lo, hi) : def); };
    maybe ("shape", 0.2f, 0.85f, 0.5f); maybe ("reverse", 0.2f, 0.9f, 0.5f); maybe ("updown", 0.1f, 0.9f, 0.5f);
    maybe ("detune", 0.05f, 0.5f, 0.0f); maybe ("width", 0.5f, 1.0f, 0.67f); maybe ("reach", 0.25f, 0.9f, 0.5f);
    maybe ("lifespeed", 0.2f, 0.8f, 0.5f); maybe ("echo", 0.2f, 0.8f, 0.5f); maybe ("size", 0.3f, 0.9f, 0.5f);
    currentPreset = -1;
}

const String SwarmProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

SwarmProcessor::SwarmProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Swarm", createLayout())
{
}

bool SwarmProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void SwarmProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedBlock = jmax (1, samplesPerBlock);
    engine.prepare (sampleRate, preparedBlock);
    limiter.prepare (sampleRate);
}

void SwarmProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());

    swarm::Settings s;
    s.ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition(); q.hasValue()) s.ppq = *q;
        }
    s.x = p ("x"); s.y = p ("y"); s.grain = p ("grain"); s.density = p ("density"); s.pitch = p ("pitch");
    s.scatter = p ("scatter"); s.life = p ("life"); s.space = p ("space"); s.tone = p ("tone"); s.dirt = p ("dirt");
    s.mix = p ("mix"); s.outDb = p ("output"); s.freeze = p ("freeze") > 0.5f;
    s.sync = roundToInt (p ("sync")); s.scale = roundToInt (p ("scale"));
    s.shape = p ("shape"); s.reverse = p ("reverse"); s.upDown = p ("updown"); s.detune = p ("detune"); s.width = p ("width");
    s.reach = p ("reach"); s.lifeSpeed = p ("lifespeed"); s.echo = p ("echo"); s.echoTime = roundToInt (p ("echotime"));
    s.size = p ("size"); s.duck = p ("duck"); s.split = roundToInt (p ("split"));
    s.bpm = lastBpm;

    // the engine works in blocks it was prepared for
    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; )
    {
        const int n = jmin (total - start, preparedBlock);
        AudioBuffer<float> part (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, n);
        engine.process (part, s);
        if (s.ppq >= 0.0) s.ppq += (double) n / getSampleRate() * lastBpm / 60.0;
        start += n;
    }
    if (buffer.getNumChannels() > 1) limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (1), total);
    else limiter.process (buffer.getWritePointer (0), buffer.getWritePointer (0), total);
}

void SwarmProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SwarmProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        state.removeProperty ("preset", nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* SwarmProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<SwarmEditor> (*this), "Swarm");
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SwarmProcessor();
}
