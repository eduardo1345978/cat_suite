#include "ProEditor.h"
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
AudioParameterFloatAttributes seconds (float most)
{
    return AudioParameterFloatAttributes().withStringFromValueFunction ([most] (float v, int)
    {
        const float s = tomb::envSeconds (v, most);
        return s < 1.0f ? String (roundToInt (s * 1000.0f)) + " ms" : String (s, 2) + " s";
    });
}
}

float CryptProcessor::rateHz (float v) { return 2000.0f * std::pow (48000.0f / 2000.0f, jlimit (0.0f, 1.0f, v)); }

AudioProcessorValueTreeState::ParameterLayout CryptProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&l] (const char* id, const char* name, float lo, float hi, float def, AudioParameterFloatAttributes a)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def, a)); };
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "wave", 1 }, "Wave", tomb::waveNames(), 1));
    f ("sub", "Sub", 0.0f, 1.0f, 0.0f, percent());
    f ("detune", "Detune", 0.0f, 1.0f, 0.0f, percent());
    f ("sweep", "Sweep", -1.0f, 1.0f, 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return std::abs (v) < 0.01f ? String ("off") : (v > 0 ? "down " : "up ") + String (roundToInt (std::abs (v) * 24.0f)) + " st"; }));
    f ("vibrato", "Vibrato", 0.0f, 1.0f, 0.0f, percent());
    f ("attack", "Attack", 0.0f, 1.0f, 0.0f, seconds (2.0f));
    f ("decay", "Decay", 0.0f, 1.0f, 0.35f, seconds (3.0f));
    f ("sustain", "Sustain", 0.0f, 1.0f, 0.7f, percent());
    f ("release", "Release", 0.0f, 1.0f, 0.2f, seconds (3.0f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "steps", 1 }, "Steps", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "chord", 1 }, "Chord", tomb::chordNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "arp", 1 }, "Arp", tomb::arpNames(), 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "arpRate", 1 }, "Arp Rate", tomb::arpRateNames(), 2));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -2, 2, 0,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v > 0 ? "+" + String (v) : String (v); })));
    f ("bits", "Bits", 2.0f, 16.0f, 16.0f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return v >= 15.9f ? String ("clean") : String (roundToInt (v)) + " bit"; }));
    f ("rate", "Rate", 0.0f, 1.0f, 1.0f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return v >= 0.995f ? String ("clean") : String (rateHz (v) / 1000.0f, 1) + " kHz"; }));
    f ("drive", "Drive", 0.0f, 1.0f, 0.0f, percent());
    f ("gloom", "Gloom", 0.0f, 1.0f, 0.25f, percent());
    f ("crypt", "Crypt", 0.0f, 1.0f, 0.35f, percent());
    f ("echo", "Echo", 0.0f, 1.0f, 0.15f, percent());
    f ("output", "Output", -12.0f, 12.0f, 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }));
    return l;
}

const std::vector<CryptProcessor::Preset>& CryptProcessor::presets()
{
    // wave: 0 p12, 1 p25, 2 square, 3 tri, 4 organ, 5 bells, 6 ghost, 7 noise, 8 metal
    // chord: 0 single, 1 minor, 2 dim, 3 power, 4 phrygian, 5 octaves, 6 sus; arp: 0 at once, 1 up, 2 down, 3 updown, 4 random
    // arpRate: 0 1/8, 1 1/16, 2 1/32, 3 1/64
    static const std::vector<Preset> list {
        { "Crypt Lead",          { { "wave", 1 }, { "vibrato", 0.3f }, { "decay", 0.4f }, { "sustain", 0.6f } } },
        { "Dungeon Bass",        { { "wave", 3 }, { "octave", -1 }, { "sub", 0.6f }, { "decay", 0.3f }, { "sustain", 0.5f }, { "crypt", 0.15f }, { "echo", 0.0f } } },
        { "Torch Arp",           { { "wave", 0 }, { "chord", 1 }, { "arp", 1 }, { "arpRate", 2 }, { "decay", 0.5f }, { "sustain", 0.4f }, { "echo", 0.3f } } },
        { "Abbey Organ",         { { "wave", 4 }, { "chord", 1 }, { "arp", 0 }, { "attack", 0.25f }, { "sustain", 0.9f }, { "release", 0.45f }, { "crypt", 0.7f }, { "gloom", 0.35f } } },
        { "Ghost Choir",         { { "wave", 6 }, { "chord", 1 }, { "arp", 0 }, { "attack", 0.45f }, { "release", 0.6f }, { "vibrato", 0.5f }, { "detune", 0.5f }, { "crypt", 0.85f }, { "gloom", 0.45f } } },
        { "Bone Bells",          { { "wave", 5 }, { "decay", 0.65f }, { "sustain", 0.0f }, { "release", 0.6f }, { "crypt", 0.6f }, { "echo", 0.35f } } },
        { "Haunted Music Box",   { { "wave", 5 }, { "octave", 1 }, { "chord", 2 }, { "arp", 3 }, { "arpRate", 1 }, { "decay", 0.6f }, { "sustain", 0.3f }, { "crypt", 0.6f }, { "echo", 0.4f } } },
        { "Boss Fight",          { { "wave", 2 }, { "chord", 2 }, { "arp", 1 }, { "arpRate", 3 }, { "drive", 0.4f }, { "sustain", 0.8f }, { "bits", 6.0f }, { "crypt", 0.2f } } },
        { "Catacomb Pad",        { { "wave", 3 }, { "chord", 4 }, { "arp", 0 }, { "attack", 0.55f }, { "release", 0.7f }, { "detune", 0.7f }, { "crypt", 0.8f }, { "gloom", 0.55f }, { "echo", 0.25f } } },
        { "Game Boy Dirge",      { { "wave", 4 }, { "bits", 4.0f }, { "rate", 0.45f }, { "vibrato", 0.4f }, { "decay", 0.5f }, { "sustain", 0.5f }, { "crypt", 0.4f } } },
        { "Rat Swarm",           { { "wave", 8 }, { "chord", 4 }, { "arp", 4 }, { "arpRate", 3 }, { "decay", 0.25f }, { "sustain", 0.2f }, { "gloom", 0.4f } } },
        { "Doom Drum",           { { "wave", 3 }, { "sweep", 0.85f }, { "decay", 0.3f }, { "sustain", 0.0f }, { "release", 0.2f }, { "octave", -1 }, { "drive", 0.35f }, { "crypt", 0.3f }, { "echo", 0.0f } } },
        { "Hi-Hat in the Dark",  { { "wave", 8 }, { "octave", 2 }, { "decay", 0.12f }, { "sustain", 0.0f }, { "release", 0.08f }, { "steps", 1 }, { "crypt", 0.25f }, { "echo", 0.0f } } },
        { "Snare of Bones",      { { "wave", 7 }, { "octave", 1 }, { "decay", 0.25f }, { "sustain", 0.0f }, { "release", 0.15f }, { "sweep", 0.3f }, { "crypt", 0.35f }, { "echo", 0.0f } } },
        { "Laser of the Damned", { { "wave", 1 }, { "sweep", -0.7f }, { "decay", 0.3f }, { "sustain", 0.0f }, { "release", 0.2f }, { "echo", 0.45f } } },
        { "Witch Hunt",          { { "wave", 0 }, { "chord", 4 }, { "arp", 2 }, { "arpRate", 2 }, { "vibrato", 0.2f }, { "detune", 0.3f }, { "echo", 0.35f }, { "crypt", 0.4f } } },
        { "Necromancer",         { { "wave", 6 }, { "octave", -1 }, { "sub", 0.5f }, { "vibrato", 0.6f }, { "drive", 0.3f }, { "attack", 0.2f }, { "crypt", 0.6f }, { "gloom", 0.5f } } },
        { "Cursed Lullaby",      { { "wave", 3 }, { "octave", 1 }, { "vibrato", 0.45f }, { "decay", 0.55f }, { "sustain", 0.35f }, { "release", 0.5f }, { "crypt", 0.7f }, { "echo", 0.45f }, { "bits", 7.0f } } },
        { "Shopkeeper",          { { "wave", 1 }, { "chord", 6 }, { "arp", 1 }, { "arpRate", 1 }, { "decay", 0.4f }, { "sustain", 0.5f }, { "crypt", 0.2f } } },
        { "Final Floor",         { { "wave", 2 }, { "chord", 3 }, { "arp", 0 }, { "detune", 0.6f }, { "sub", 0.4f }, { "drive", 0.5f }, { "bits", 8.0f }, { "crypt", 0.45f }, { "gloom", 0.3f } } },
    };
    return list;
}

void CryptProcessor::applyPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "output") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) index].values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

const String CryptProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

CryptProcessor::CryptProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Crypt", createLayout())
{
    applyPreset (0);
    sfxSeed = (juce::uint32) Time::currentTimeMillis();
    sfx = tomb::generate (tomb::Kind::Sword, sfxSeed, sfxDarkness);
    sfxFolder = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Crypt Sounds");
}

bool CryptProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void CryptProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate);
    crusher.prepare (sampleRate);
    space.prepare (sampleRate, samplesPerBlock);
    limiter.prepare (sampleRate);
    keyboard.reset();
    monoSpare.assign ((size_t) jmax (1, samplesPerBlock), 0.0f);
}

void CryptProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboard.processNextMidiBuffer (midi, 0, n, true);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm(); b.hasValue() && *b > 20.0) lastBpm = *b;

    tomb::SynthSettings s;
    s.wave = (tomb::Wave) jlimit (0, 8, roundToInt (p ("wave")));
    s.sub = p ("sub"); s.detune = p ("detune"); s.sweep = p ("sweep"); s.vibrato = p ("vibrato");
    s.attack = p ("attack"); s.decay = p ("decay"); s.sustain = p ("sustain"); s.release = p ("release");
    s.steps = p ("steps") > 0.5f;
    s.chord = (tomb::Chord) jlimit (0, 6, roundToInt (p ("chord")));
    s.arp = (tomb::Arp) jlimit (0, 4, roundToInt (p ("arp")));
    s.arpRate = jlimit (0, 3, roundToInt (p ("arpRate")));
    s.octave = jlimit (-2, 2, roundToInt (p ("octave")));
    s.bpm = lastBpm;
    synth.setSettings (s);

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool mono = r == nullptr;
    if (mono)
    {
        if ((int) monoSpare.size() < n) monoSpare.resize ((size_t) n);
        std::fill (monoSpare.begin(), monoSpare.begin() + n, 0.0f);
        r = monoSpare.data();
    }

    int at = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = jlimit (0, n, meta.samplePosition);
        if (pos > at) { synth.render (l + at, r + at, pos - at); at = pos; }
        if (m.isNoteOn()) synth.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) synth.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel()) synth.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isSustainPedalOn()) synth.sustain (true);
        else if (m.isSustainPedalOff()) synth.sustain (false);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
    }
    if (at < n) synth.render (l + at, r + at, n - at);

    crusher.process (l, r, n, p ("bits"), p ("rate") >= 0.995f ? (float) getSampleRate() : rateHz (p ("rate")));
    space.process (l, r, n, p ("gloom"), p ("drive"), p ("crypt"), p ("echo"), lastBpm);
    const float gain = Decibels::decibelsToGain (p ("output"));
    for (int i = 0; i < n; ++i) { l[i] *= gain; r[i] *= gain; }

    // the sound-effect preview (not through the synth's crypt: the effect has its own room)
    if (previewLock.tryEnter())
    {
        if (previewAt < preview.getNumSamples())
        {
            const int m = jmin (n, preview.getNumSamples() - previewAt);
            const float* src = preview.getReadPointer (0, previewAt);
            for (int i = 0; i < m; ++i) { l[i] += src[i] * 0.8f; r[i] += src[i] * 0.8f; }
            previewAt += m;
            previewPos = previewAt >= preview.getNumSamples() ? -1.0f : (float) previewAt / (float) preview.getNumSamples();
        }
        previewLock.exit();
    }

    limiter.process (l, r, n);
    float pk = 0.0f;
    for (int i = 0; i < n; ++i) pk = jmax (pk, std::abs (l[i]));
    level = level.load() * 0.7f + pk * 0.3f;
    if (mono) for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + r[i]);
}

//==============================================================================
void CryptProcessor::newSfx (tomb::Kind k)
{
    history.insert (history.begin(), sfx);
    if (history.size() > 8) history.resize (8);
    sfx = tomb::generate (k, ++sfxSeed * 2654435761u, sfxDarkness);
}

void CryptProcessor::mutateSfx()
{
    history.insert (history.begin(), sfx);
    if (history.size() > 8) history.resize (8);
    sfx = tomb::mutate (sfx, ++sfxSeed * 2246822519u);
}

AudioBuffer<float> CryptProcessor::renderSfx (double sampleRate) const
{
    return tomb::render (sfx, sampleRate, sfxPitch, sfxLength);
}

void CryptProcessor::playPreview (AudioBuffer<float> b)
{
    AudioBuffer<float> old;
    {
        const SpinLock::ScopedLockType lock (previewLock);
        std::swap (old, preview);
        preview = std::move (b);
        previewAt = 0;
    }
    previewPos = 0.0f;
}   // `old` goes away here, on this (the message) thread

File CryptProcessor::saveSfx (const File& file)
{
    File target = file;
    if (target == File())
    {
        int& next = sfxCounter[(int) sfx.kind];
        if (next == 0) next = 1;
        const String base = tomb::kindFileName (sfx.kind);
        do target = sfxFolder.getChildFile (base + "_" + String (next++).paddedLeft ('0', 2) + ".wav");
        while (target.existsAsFile());
    }
    const auto b = renderSfx (44100.0);
    return tomb::saveWav (b, 44100.0, target) ? target : File();
}

File CryptProcessor::makePack()
{
    const auto dir = sfxFolder.getChildFile ("Crypt pack " + Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S"));
    for (int k = 0; k < (int) tomb::Kind::count; ++k)
        for (int v = 1; v <= 3; ++v)
        {
            const auto s = tomb::generate ((tomb::Kind) k, ++sfxSeed * 2654435761u, sfxDarkness);
            const auto b = tomb::render (s, 44100.0, sfxPitch, sfxLength);
            tomb::saveWav (b, 44100.0, dir.getChildFile (tomb::kindFileName ((tomb::Kind) k) + "_" + String (v).paddedLeft ('0', 2) + ".wav"));
        }
    return dir;
}

//==============================================================================
void CryptProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.setProperty ("sfx", JSON::toString (sfx.toVar(), true), nullptr);
    state.setProperty ("sfxDark", sfxDarkness, nullptr);
    state.setProperty ("sfxPitch", sfxPitch, nullptr);
    state.setProperty ("sfxLength", sfxLength, nullptr);
    state.setProperty ("sfxFolder", sfxFolder.getFullPathName(), nullptr);
    state.setProperty ("tab", tab, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void CryptProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto state = ValueTree::fromXml (*xml);
        currentPreset = (int) state.getProperty ("preset", -1);
        if (state.hasProperty ("sfx")) sfx = tomb::Sfx::fromVar (JSON::parse (state.getProperty ("sfx").toString()));
        sfxDarkness = jlimit (0.0f, 1.0f, (float) state.getProperty ("sfxDark", 0.5f));
        sfxPitch = jlimit (-12.0f, 12.0f, (float) state.getProperty ("sfxPitch", 0.0f));
        sfxLength = jlimit (0.5f, 2.0f, (float) state.getProperty ("sfxLength", 1.0f));
        if (const String f = state.getProperty ("sfxFolder").toString(); f.isNotEmpty() && File::isAbsolutePath (f)) sfxFolder = File (f);
        tab = jlimit (0, 1, (int) state.getProperty ("tab", 0));
        for (auto id : { "preset", "sfx", "sfxDark", "sfxPitch", "sfxLength", "sfxFolder", "tab" }) state.removeProperty (id, nullptr);
        apvts.replaceState (state);
    }
}

AudioProcessorEditor* CryptProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<CryptEditor> (*this), "Crypt");
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return cat::makePlugin<CryptProcessor>();
}
