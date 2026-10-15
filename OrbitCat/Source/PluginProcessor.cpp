#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"
#include "Surprise.h"

using namespace orbitcat;

juce::AudioProcessorValueTreeState::ParameterLayout OrbitCatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto pct = [&] (const String& id, const String& name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                                           AudioParameterFloatAttributes().withLabel ("%")));
    };
    auto fl = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, const char* unit)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel (unit)));
    };

    pct ("gravity", "Gravity", 50.0f);
    pct ("drag", "Drag", 0.0f);
    pct ("turbulence", "Turbulence", 0.0f);
    fl ("align", "Align", NormalisableRange<float> (0.0f, 359.0f, 1.0f), 90.0f, "deg");
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "spokes", 1 }, "Spokes", 1, 6, 1));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "accent", 1 }, "Accent", 1, 127, 100));
    fl ("tilt", "Tilt", NormalisableRange<float> (0.0f, 80.0f, 1.0f), 30.0f, "deg");
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "quantize", 1 }, "Quantize", getQuantizeNames(), 3));
    pct ("glow", "Glow", 60.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "latch", 1 }, "Latch", false));
    pct ("swing", "Swing", 0.0f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "followGroove", 1 }, "Follow Drum Groove", false));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "outChannel", 1 }, "Output Channel", 0, 16, 0,
                                                     AudioParameterIntAttributes().withStringFromValueFunction (
                                                         [] (int v, int) { return v == 0 ? String ("As input") : String (v); })));

    for (int s = 0; s < numSlots; ++s)
    {
        const String n = " " + String (s + 1);
        fl (slotParam (s, "mass"), "Mass" + n, NormalisableRange<float> (1.0f, 100.0f, 1.0f), 50.0f, "kg");
        fl (slotParam (s, "radius"), "Radius" + n, NormalisableRange<float> (1.0f, 32.0f, 0.1f, 0.5f), 4.0f + 2.0f * (float) s, "AU");
        fl (slotParam (s, "ecc"), "Eccentricity" + n, NormalisableRange<float> (0.0f, 0.9f, 0.01f), 0.1f, "");
        fl (slotParam (s, "speed"), "Speed" + n, NormalisableRange<float> (0.25f, 4.0f, 0.01f, 0.5f), 1.0f, "x");
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { slotParam (s, "oct"), 1 }, "Octave" + n, -2, 2, 0));
        pct (slotParam (s, "damp"), "Damp" + n, 70.0f);
        fl (slotParam (s, "gate"), "Gate" + n, NormalisableRange<float> (0.02f, 2.0f, 0.01f, 0.5f), 0.2f, "s");
        pct (slotParam (s, "boost"), "Boost" + n, 15.0f);
    }
    return layout;
}

OrbitCatProcessor::OrbitCatProcessor()
    : AudioProcessor (BusesProperties()
#if CAT_INSTRUMENT_MODE && ! JucePlugin_IsMidiEffect
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                      ),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    {
        cat::LinkClient::Caps caps;
        caps.receiveChords = true;
        caps.receiveChordsText = "Orbit Chord-Cat's chords";
        caps.grooveParam = "followGroove";
        presetManager.enableLink (apvts, caps);
    }
    scheduler.monitor = &noteMonitor;
    heldKeys.reserve (64);

    const char* globalIds[] = { "gravity", "drag", "turbulence", "align", "spokes", "accent", "quantize", "latch", "outChannel" };
    for (int i = 0; i < 9; ++i)
        gRefs[i] = apvts.getRawParameterValue (globalIds[i]);
    const char* slotIds[] = { "mass", "radius", "ecc", "speed", "oct", "damp", "gate", "boost" };
    for (int s = 0; s < numSlots; ++s)
        for (int i = 0; i < 8; ++i)
            slotRefs[(size_t) s].v[i] = apvts.getRawParameterValue (slotParam (s, slotIds[i]));
    swingRef = apvts.getRawParameterValue ("swing");
    cat::followDrumGroove (presetManager, apvts, [this] (const cat::CatLink::Groove& g)
    {
        cat::SurpriseControls::setParam (*this, "swing", g.swing);
    });
    applyPreset (getOrbitPresets().size() > 14 ? 14 : 0);   // "Quiet Orbit": a calm start
}

OrbitCatProcessor::~OrbitCatProcessor() = default;

bool OrbitCatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono() || out.isDisabled();
#endif
}

void OrbitCatProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    hostSync.prepare (sr);
    scheduler.prepare (8192);
    outBuffer.ensureSize ((size_t) juce::jmax (4096, samplesPerBlock * 4));
    sampleCounter = 0;
    engine.reset();
    heldKeys.clear();
}

void OrbitCatProcessor::readParams()
{
    auto g = [this] (int i) { return gRefs[i]->load(); };
    global.gravity = g (0) / 100.0f;
    global.drag = g (1) / 100.0f;
    global.turbulence = g (2) / 100.0f;
    global.alignRadians = juce::degreesToRadians ((double) g (3));
    global.spokes = juce::roundToInt (g (4));
    accent = g (5);
    quantize = quantizeToPpq (juce::roundToInt (g (6)));
    latch = g (7) > 0.5f;
    outChannel = juce::roundToInt (g (8));

    for (int s = 0; s < numSlots; ++s)
    {
        auto v = [&] (int i) { return slotRefs[(size_t) s].v[i]->load(); };
        auto& b = slots[(size_t) s];
        b.mass = v (0);
        b.radius = v (1);
        b.eccentricity = v (2);
        b.speed = v (3);
        b.octave = juce::roundToInt (v (4));
        b.damp = v (5) / 100.0f;
        b.gate = v (6);
        b.boost = v (7) / 100.0f;
    }
}

void OrbitCatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    presetManager.linkClient->processInput (midi, buffer.getNumSamples());     // Cat Link: Chord-Cat's chord
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    hostSync.beginBlock (getPlayHead(), numSamples);
    midiLearn.processMidi (midi);
    readParams();

    int lastPos = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);
        advance (lastPos, pos);
        const auto m = meta.getMessage();

        if (m.isNoteOn())
        {
            // Latch: a new chord (no keys down) replaces the orbiting one
            if (latch && heldKeys.empty())
                engine.reset();
            heldKeys.emplace_back (m.getChannel(), m.getNoteNumber());
            if (! hostSync.isRunning())
                hostSync.startFreeRunAt (pos);
            engine.addNote (m.getNoteNumber(), m.getChannel(), global, slots, rng);
        }
        else if (m.isNoteOff())
        {
            heldKeys.erase (std::remove (heldKeys.begin(), heldKeys.end(), std::make_pair (m.getChannel(), m.getNoteNumber())), heldKeys.end());
            if (! latch)
                engine.removeNote (m.getNoteNumber(), m.getChannel());
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldKeys.clear();
            engine.reset();
            scheduler.allNotesOff (outBuffer, pos);
            scheduler.schedule (sampleCounter + pos, m);
        }
        else
        {
            scheduler.schedule (sampleCounter + pos, m);
        }
        lastPos = pos;
    }
    advance (lastPos, numSamples);

    if (engine.numActive() == 0 && hostSync.isFreeRunning())
        hostSync.stopFreeRun();

    outBuffer.clear();
    scheduler.emit (outBuffer, sampleCounter, numSamples);
    midi.swapWith (outBuffer);

    // Display feed
    const float decay = std::exp (-(float) numSamples / (float) sampleRate * 5.0f);
    for (int i = 0; i < maxBodies; ++i)
    {
        const auto& b = engine.bodies[(size_t) i];
        auto& v = bodyViews[(size_t) i];
        flashes[(size_t) i] *= decay;
        v.active = b.active;
        if (! b.active) continue;
        const auto& p = slots[(size_t) b.slot];
        v.x = (float) (b.radiusNow * std::cos (b.theta));
        v.y = (float) (b.radiusNow * std::sin (b.theta));
        v.semiMajor = (float) b.semiMajor;
        v.eccentricity = p.eccentricity;
        v.omega = (float) b.omega;
        v.mass = p.mass;
        v.note = b.note;
        v.slot = b.slot;
        v.flash = flashes[(size_t) i];
    }
    uiPpq = hostSync.ppqAt (0);
    uiTransportRunning = hostSync.isRunning();

    hostSync.endBlock();
    sampleCounter += numSamples;
}

void OrbitCatProcessor::advance (int fromSample, int toSample)
{
    if (toSample <= fromSample || ! hostSync.isRunning() || engine.numActive() == 0)
        return;

    constexpr int controlStep = 32;
    for (int s = fromSample; s < toSample; s += controlStep)
    {
        const int n = juce::jmin (controlStep, toSample - s);
        const double dt = hostSync.get().ppqPerSample * n;

        engine.step (dt, global, slots, [&] (int index, double fraction, double speedRatio)
        {
            const auto& b = engine.bodies[(size_t) index];
            const auto& p = slots[(size_t) b.slot];
            int pos = s + (int) std::floor (fraction * n);
            int64_t when = sampleCounter + pos;

            // Quantize: wait for the next grid line of the host grid
            if (quantize > 0.0)
            {
                const double ppq = hostSync.ppqAt (pos);
                const double q = std::ceil (ppq / quantize - 1.0e-9) * quantize;
                // Swing: every second grid line moves later (same feel as Drum-Cat / Patt-Cat)
                const bool offBeat = ((int64_t) std::llround (q / quantize)) % 2 != 0;
                const double swung = offBeat ? q + swingRef->load() / 100.0 * 0.5 * quantize : q;
                when = sampleCounter + (int64_t) std::llround (hostSync.sampleForPpq (swung));
            }

            const int note = b.note + 12 * p.octave;
            const int channel = outChannel > 0 ? outChannel : b.channel;
            if (note < 0 || note > 127)
                return;

            // Heavier bodies are louder; Boost adds accent when a body races past the sun
            const float massFactor = 0.75f + 0.25f * p.mass / 100.0f;
            const float boost = 1.0f + p.boost * juce::jlimit (-0.6f, 1.5f, (float) speedRatio - 1.0f);
            const int vel = juce::jlimit (1, 127, juce::roundToInt (accent * massFactor * boost));
            const int64_t off = when + juce::jmax ((int64_t) 1, (int64_t) (p.gate * sampleRate));

            scheduler.releaseNote (channel, note, when);    // re-strike cleanly if it is still ringing
            scheduler.scheduleNote (when, off, channel, note, vel);
            flashes[(size_t) index] = 1.0f;
        });
    }
}

//==============================================================================
void OrbitCatProcessor::applyPreset (int index)
{
    const auto& presets = getOrbitPresets();
    index = juce::jlimit (0, (int) presets.size() - 1, index);
    currentPreset = index;
    const auto& p = presets[(size_t) index];
    auto set = [this] (const juce::String& id, float v) { cat::SurpriseControls::setParam (*this, id, v); };

    set ("gravity", p.global.gravity * 100.0f);
    set ("drag", p.global.drag * 100.0f);
    set ("turbulence", p.global.turbulence * 100.0f);
    set ("align", p.alignDegrees);
    set ("spokes", (float) p.global.spokes);
    set ("tilt", p.tilt);
    set ("glow", p.glow * 100.0f);
    set ("quantize", (float) p.quantize);
    for (int s = 0; s < numSlots; ++s)
    {
        const auto& b = p.bodies[(size_t) s];
        set (slotParam (s, "mass"), b.mass);
        set (slotParam (s, "radius"), b.radius);
        set (slotParam (s, "ecc"), b.eccentricity);
        set (slotParam (s, "speed"), b.speed);
        set (slotParam (s, "oct"), (float) b.octave);
        set (slotParam (s, "damp"), b.damp * 100.0f);
        set (slotParam (s, "gate"), b.gate);
        set (slotParam (s, "boost"), b.boost * 100.0f);
    }
}

void OrbitCatProcessor::surprise()
{
    // Random gravitational relationships - fast to discover, slow to program by hand
    auto& r = juce::Random::getSystemRandom();
    auto set = [this] (const juce::String& id, float v) { cat::SurpriseControls::setParam (*this, id, v); };
    set ("gravity", 20.0f + r.nextFloat() * 80.0f);
    set ("drag", r.nextFloat() < 0.7f ? 0.0f : r.nextFloat() * 80.0f);
    set ("turbulence", r.nextFloat() < 0.5f ? 0.0f : r.nextFloat() * 70.0f);
    set ("align", (float) r.nextInt (360));
    set ("spokes", (float) (1 + r.nextInt (4)));
    set ("quantize", (float) (r.nextFloat() < 0.25f ? 0 : 2 + r.nextInt (5)));
    set ("tilt", 10.0f + r.nextFloat() * 60.0f);
    const float baseRadius = 2.0f + r.nextFloat() * 8.0f;
    for (int s = 0; s < numSlots; ++s)
    {
        set (slotParam (s, "mass"), 10.0f + r.nextFloat() * 90.0f);
        set (slotParam (s, "radius"), juce::jlimit (1.0f, 32.0f, baseRadius + (float) s * (0.5f + r.nextFloat() * 3.0f)));
        set (slotParam (s, "ecc"), r.nextFloat() < 0.4f ? r.nextFloat() * 0.15f : r.nextFloat() * 0.8f);
        set (slotParam (s, "speed"), r.nextFloat() < 0.7f ? 1.0f : 0.5f + r.nextFloat() * 1.5f);
        set (slotParam (s, "oct"), (float) (r.nextFloat() < 0.7f ? 0 : r.nextInt (3) - 1));
        set (slotParam (s, "damp"), r.nextFloat() * 100.0f);
        set (slotParam (s, "gate"), 0.05f + r.nextFloat() * 0.4f);
        set (slotParam (s, "boost"), r.nextFloat() * 60.0f);
    }
}

//==============================================================================
void OrbitCatProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("ORBITCAT");
    root.setProperty ("preset", currentPreset.load(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (midiLearn.toValueTree(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void OrbitCatProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("ORBITCAT")) return;
    currentPreset = (int) root.getProperty ("preset", 0);
    if (auto params = root.getChildWithName (apvts.state.getType()); params.isValid())
        apvts.replaceState (params);
    midiLearn.fromValueTree (root.getChildWithName (cat::MidiLearnManager::treeId));
}

juce::AudioProcessorEditor* OrbitCatProcessor::createEditor()
{
    return new cat::ScaledEditor (*this, std::make_unique<OrbitCatEditor> (*this), "Orbit-Cat");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrbitCatProcessor();
}
