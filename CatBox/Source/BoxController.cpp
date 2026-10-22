#include "BoxController.h"
#include <cmath>
#include <algorithm>

namespace catbox
{
BoxController::BoxController (juce::File dataDirectory) : dataDir (std::move (dataDirectory))
{
    juce::WeakReference<BoxController> init (this);   // creates the shared pointer here, not on the MIDI thread
    for (auto& p : padHeldNote) p = -1;
    for (auto& p : padHeldInst) p = -1;

    dataDir.createDirectory();
    const auto last = dataDir.getChildFile ("last.json");
    if (last.existsAsFile()) data.fromVar (juce::JSON::parse (last));

    syncAtomics();
    regenerateDrums();
    regenerateBass();
    sendBassChord();
    publish();
    startTimerHz (30);
}

BoxController::~BoxController()
{
    stopTimer();
    if (dirty) saveState();
}

void BoxController::syncAtomics()
{
    page = data.page;
    key = juce::roundToInt (data.globals[GKey]);
    scale = juce::roundToInt (data.globals[GScale]);
    for (int i = 0; i < NumInstruments; ++i) padOct[(size_t) i] = data.padOctave[(size_t) i];
}

void BoxController::runOnMessageThread (std::function<void()> fn)
{
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        fn();
        return;
    }
    juce::WeakReference<BoxController> weak (this);
    juce::MessageManager::callAsync ([weak, f = std::move (fn)]
    {
        if (weak.get() != nullptr) f();
    });
}

void BoxController::setFlash (const juce::String& s)
{
    flash = s;
    flashTime = juce::Time::getMillisecondCounter();
}

juce::String BoxController::getFlash() const
{
    return juce::Time::getMillisecondCounter() - flashTime < 1600 ? flash : juce::String();
}

//==============================================================================
void BoxController::turnKnob (int knob, float clicks)
{
    if (knob < 0 || knob >= numKnobs || clicks == 0.0f) return;
    const int pg = page.load();
    const int s = knob + (shift.load() ? numKnobs : 0);
    const auto& d = paramDesc (pg, s);
    if (! d.used()) return;

    float value = data.get (pg, s);
    if (d.kind == Kind::Percent)
    {
        value += clicks * d.step() * (d.max - d.min);
    }
    else
    {
        // choices and numbers move one whole step per click; mouse drags send fractions
        auto& rem = knobRemainder[(size_t) s];
        rem += clicks;
        const float whole = std::trunc (rem);
        if (whole == 0.0f) return;
        rem -= whole;
        value += whole * d.step();
    }
    const float before = data.get (pg, s);
    data.set (pg, s, value);
    if (data.get (pg, s) != before) paramChanged (pg, s);
}

void BoxController::paramChanged (int inst, int s)
{
    if (s == slot::Key || s == slot::Scale)
    {
        key = juce::roundToInt (data.globals[GKey]);
        scale = juce::roundToInt (data.globals[GScale]);
        sendBassChord();
    }
    if (inst == Drums && (s == slot::DStyle || s == slot::DDensity || s == slot::DFills || s == slot::DBars || s == slot::DSeed))
        regenerateDrums();
    if (inst == Bass && (s == slot::BStyle || s == slot::BDensity || s == slot::BGhosts || s == slot::BSlides
                         || s == slot::BBars || s == slot::BSeed || s == slot::BFills))
        regenerateBass();
    publish();
    markDirty();
}

void BoxController::regenerateDrums()
{
    const auto& v = data.inst[Drums].values;
    drumcat::GeneratorSettings g;
    g.style = juce::roundToInt (v[slot::DStyle]);
    g.bars = barsFromChoice (v[slot::DBars]);
    g.density = v[slot::DDensity];
    g.fills = v[slot::DFills];
    g.seed = juce::roundToInt (v[slot::DSeed]);
    drumPattern = drumcat::generatePattern (g);
}

void BoxController::regenerateBass()
{
    const auto& v = data.inst[Bass].values;
    basscat::GeneratorSettings g;
    g.style = juce::roundToInt (v[slot::BStyle]);
    g.bars = barsFromChoice (v[slot::BBars]);
    g.density = v[slot::BDensity];
    g.ghosts = v[slot::BGhosts];
    g.slides = v[slot::BSlides];
    g.fills = v[slot::BFills];
    g.seed = juce::roundToInt (v[slot::BSeed]) * 7919;
    bassPattern = basscat::generatePattern (g);
}

void BoxController::publish()
{
    AudioSnapshot s;
    const auto& d = data.inst[Drums].values;
    const auto& a = data.inst[Arp].values;
    const auto& b = data.inst[Bass].values;

    s.bpm = data.globals[GTempo];
    s.master = data.globals[GMaster];
    s.muted = data.muted;
    s.volume = { d[slot::DVolume], a[slot::AVolume], b[slot::BVolume] };

    s.fx[Drums].filter = 1.0f - d[slot::DFilter];
    s.fx[Drums].drive = d[slot::DDrive];
    s.fx[Drums].chorus = d[slot::DChorus];
    s.fx[Drums].delay = d[slot::DEcho];
    s.fx[Drums].crush = d[slot::DCrush];
    s.fx[Arp].filter = 1.0f - a[slot::AFilter];
    s.fx[Arp].drive = a[slot::ADrive];
    s.fx[Arp].chorus = a[slot::AChorus];
    s.fx[Arp].delay = a[slot::AEcho];
    s.fx[Bass].filter = 1.0f - b[slot::BFilter];
    s.fx[Bass].drive = b[slot::BDrive];
    s.fx[Bass].chorus = b[slot::BChorus];
    s.fx[Bass].delay = b[slot::BEcho];

    // drums: the generated groove (swing + human feel) + what was played in with Rec
    const double loop = barsFromChoice (d[slot::DBars]) * 4.0;
    const double swingPush = d[slot::DSwing] * 0.125;   // up to half a 16th
    auto swing = [swingPush] (double pos)
    {
        const double sixteenth = pos * 4.0;
        const double nearest = std::round (sixteenth);
        return (std::abs (sixteenth - nearest) < 0.01 && ((int) nearest & 1) != 0) ? swingPush : 0.0;
    };
    for (const auto& h : drumPattern.hits)
    {
        if (h.pos >= loop) continue;
        const double ppq = std::fmod (h.pos + swing (h.pos) + h.micro * d[slot::DHuman] * 2.0 + loop, loop);
        s.drumHits.push_back ({ ppq, drumcat::noteFor (0, h.lane, h.variant), h.velocity });
    }
    for (const auto& h : data.inst[Drums].drumHits)
    {
        const double pos = h.step * 0.25;
        if (pos < loop) s.drumHits.push_back ({ pos + swing (pos), h.note, h.velocity });
    }
    std::sort (s.drumHits.begin(), s.drumHits.end(), [] (const SeqHit& x, const SeqHit& y) { return x.ppq < y.ppq; });
    s.drumLoop = loop;
    s.drumKit = juce::roundToInt (d[slot::DKit]);

    s.bassNotes = bassPattern.notes;
    s.bassLoop = bassPattern.lengthPpq();
    s.bassSound = juce::roundToInt (b[slot::BSound]);
    s.bassLowNote = 33 + 12 * juce::roundToInt (b[slot::BOctave]);

    s.arp.division = juce::roundToInt (a[slot::ADivision]);
    s.arp.mode = juce::roundToInt (a[slot::AMode]);
    s.arp.octaves = juce::roundToInt (a[slot::AOctaves]);
    s.arp.gate = a[slot::AGate];
    s.arp.swing = a[slot::ASwing];
    s.arp.length = juce::roundToInt (a[slot::ALength]);
    s.arp.ratchet = juce::roundToInt (a[slot::ARatchet]);
    s.arp.probability = a[slot::AProbability];
    s.arp.accent = a[slot::AAccent];
    s.arp.steps = data.inst[Arp].arpSteps;
    s.arpSound = juce::roundToInt (a[slot::ASound]);

    audio.setSnapshot (s);
}

void BoxController::sendBassChord()
{
    const int k = key.load(), sc = scale.load(), deg = data.bassChordDegree;
    const std::vector<int> notes { scaleNote (k, sc, deg, 48), scaleNote (k, sc, deg + 2, 48), scaleNote (k, sc, deg + 4, 48) };
    const auto c = basscat::analyseChord (notes);
    BoxEvent e;
    e.type = BoxEvent::BassChord;
    e.a = c.valid ? c.rootPc : notes[0] % 12;
    e.b = c.third;
    e.c = c.fifth;
    e.d = c.seventh;
    e.e = c.hasThird ? 1 : 0;
    audio.post (e);
}

juce::String BoxController::bassChordName() const
{
    return degreeChordName (key.load(), scale.load(), data.bassChordDegree);
}

int BoxController::arpPadNote (int pad) const
{
    return scaleNote (key.load(), scale.load(), pad, 48 + 12 * padOct[Arp].load());
}

int BoxController::bassPadNote (int pad) const
{
    return scaleNote (key.load(), scale.load(), pad, 36 + 12 * padOct[Bass].load());
}

//==============================================================================
void BoxController::padDown (int pad, int velocity)
{
    if (pad < 0 || pad >= numPads) return;
    velocity = juce::jlimit (1, 127, velocity);

    if (shift.load())
    {
        padHeldInst[(size_t) pad] = -1;
        const bool save = rec.load() || saveArm.exchange (false);
        const int pg = page.load();
        runOnMessageThread ([this, save, pg, pad] { save ? savePreset (pg, pad) : loadPreset (pg, pad); });
        return;
    }

    const int pg = page.load();
    padHeldInst[(size_t) pad] = pg;
    BoxEvent e;
    switch (pg)
    {
        case Drums:
            e.type = BoxEvent::DrumHit;
            e.a = drumPadNote (pad);
            e.b = velocity;
            audio.post (e);
            break;

        case Arp:
            if (stepView.load())
            {
                padHeldInst[(size_t) pad] = -1;
                runOnMessageThread ([this, pad]
                {
                    auto& st = data.inst[Arp].arpSteps[(size_t) pad];
                    st = (uint8_t) ((st + 1) % NumStepStates);
                    publish();
                    markDirty();
                });
                break;
            }
            e.type = BoxEvent::ArpOn;
            e.a = arpPadNote (pad);
            e.b = velocity;
            padHeldNote[(size_t) pad] = e.a;
            audio.post (e);
            break;

        case Bass:
        default:
            runOnMessageThread ([this, pad]
            {
                data.bassChordDegree = pad;
                sendBassChord();
                markDirty();
            });
            if (! audio.isPlaying())   // stopped: the pad also plays the chord's root
            {
                e.type = BoxEvent::BassOn;
                e.a = bassPadNote (pad);
                e.b = velocity;
                padHeldNote[(size_t) pad] = e.a;
                audio.post (e);
            }
            break;
    }
}

void BoxController::padUp (int pad)
{
    if (pad < 0 || pad >= numPads) return;
    const int inst = padHeldInst[(size_t) pad].exchange (-1);
    const int note = padHeldNote[(size_t) pad].exchange (-1);
    if (inst < 0 || note < 0) return;
    BoxEvent e;
    e.type = inst == Arp ? BoxEvent::ArpOff : BoxEvent::BassOff;
    e.a = note;
    if (inst == Arp || inst == Bass) audio.post (e);
}

//==============================================================================
void BoxController::buttonDown (int b)
{
    const int pg = page.load();
    switch (b)
    {
        case BtnPlay:
            audio.setPlaying (! audio.isPlaying());
            break;

        case BtnHold:
        {
            latchOn = ! latchOn;
            BoxEvent e; e.type = BoxEvent::ArpLatch; e.a = latchOn ? 1 : 0;
            audio.post (e);
            setFlash (latchOn ? sk ("Hold zapnutý: arp hrá aj po pustení") : sk ("Hold vypnutý"));
            break;
        }

        case BtnShift:
            shift = true;
            break;

        case BtnTap:
            if (shift.load())
            {
                if (pg == Drums)
                {
                    data.inst[Drums].drumHits.clear();
                    publish();
                    markDirty();
                    setFlash (sk ("Nahraté údery zmazané"));
                }
                else if (pg == Arp)
                {
                    BoxEvent e; e.type = BoxEvent::ArpClear;
                    audio.post (e);
                    setFlash (sk ("Arp: tóny pustené"));
                }
            }
            else tapTempo();
            break;

        case BtnInstrument:
            if (shift.load())
            {
                auto& m = data.muted[(size_t) pg];
                m = ! m;
                publish();
                markDirty();
                setFlash (instrumentName (pg) + (m ? sk (": stlmené") : sk (": znie")));
            }
            else
            {
                data.page = (pg + 1) % NumInstruments;
                page = data.page;
                markDirty();
            }
            break;

        case BtnOctDown:
        case BtnOctUp:
        {
            const int dir = b == BtnOctUp ? 1 : -1;
            if (pg == Drums)
            {
                auto& seed = data.inst[Drums].values[slot::DSeed];
                seed = (float) (((juce::roundToInt (seed) - 1 + dir + 99) % 99) + 1);
                paramChanged (Drums, slot::DSeed);
                setFlash (sk ("Variácia rytmu ") + juce::String (juce::roundToInt (seed)));
            }
            else
            {
                auto& o = data.padOctave[(size_t) pg];
                o = juce::jlimit (-2, 2, o + dir);
                padOct[(size_t) pg] = o;
                markDirty();
                setFlash (sk ("Oktáva padov ") + (o > 0 ? "+" : "") + juce::String (o));
            }
            break;
        }

        case BtnRec:
            rec = true;
            if (shift.load())
            {
                saveArm = true;   // the next pad saves (also when Rec is already let go, e.g. clicked with the mouse)
                setFlash (sk ("Ukladanie: stlač pad (1-16)"));
                break;
            }
            if (pg == Drums)
            {
                audio.setRecording (! audio.isRecording());
                setFlash (audio.isRecording() ? (audio.isPlaying() ? sk ("Rec: hraj na pady") : sk ("Rec: stlač Play a hraj na pady"))
                                              : sk ("Rec vypnutý"));
            }
            else if (pg == Arp)
            {
                stepView = ! stepView.load();
                setFlash (stepView.load() ? sk ("Kroky: pady menia kroky patternu") : sk ("Pady hrajú tóny"));
            }
            else
            {
                setFlash (sk ("Bass: pady menia akord, bass hrá sám"));
            }
            break;

        default: break;
    }
}

void BoxController::buttonUp (int b)
{
    if (b == BtnShift) { shift = false; saveArm = false; }
    if (b == BtnRec) rec = false;
}

bool BoxController::isButtonLit (int b) const
{
    switch (b)
    {
        case BtnPlay:  return audio.isPlaying();
        case BtnHold:  return latchOn;
        case BtnShift: return shift.load();
        case BtnRec:   return page.load() == Arp ? stepView.load() : audio.isRecording();
        default:       return false;
    }
}

void BoxController::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.isEmpty() && now - taps.getLast() > 2000.0) taps.clear();
    taps.add (now);
    while (taps.size() > 5) taps.remove (0);
    if (taps.size() >= 2)
    {
        const double avg = (taps.getLast() - taps.getFirst()) / (taps.size() - 1);
        data.set (Drums, slot::Tempo, (float) (60000.0 / avg));
        paramChanged (Drums, slot::Tempo);
        setFlash ("Tempo " + juce::String (juce::roundToInt (data.globals[GTempo])) + " BPM");
    }
    else setFlash (sk ("Ťukaj do rytmu..."));
}

//==============================================================================
void BoxController::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.getChannel() == controllerChannel)
    {
        if (m.isNoteOn() && m.getNoteNumber() >= firstPadNote && m.getNoteNumber() < firstPadNote + numPads)
            padDown (m.getNoteNumber() - firstPadNote, m.getVelocity());
        else if (m.isNoteOff() && m.getNoteNumber() >= firstPadNote && m.getNoteNumber() < firstPadNote + numPads)
            padUp (m.getNoteNumber() - firstPadNote);
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            if (cc >= firstKnobCC && cc < firstKnobCC + numKnobs)
            {
                const int clicks = v == 0 || v == 64 ? 0 : (v < 64 ? v : v - 128);
                runOnMessageThread ([this, k = cc - firstKnobCC, clicks] { turnKnob (k, (float) clicks); });
            }
            else if (cc >= firstButtonCC && cc < firstButtonCC + NumButtons)
            {
                const int b = cc - firstButtonCC;
                const bool down = v >= 64;
                if (b == BtnShift) shift = down;            // right away: a pad may follow immediately
                if (b == BtnRec && shift.load()) rec = down;
                runOnMessageThread ([this, b, down] { down ? buttonDown (b) : buttonUp (b); });
            }
        }
        return;
    }

    // a normal MIDI keyboard plays the instrument on screen
    if (! (m.isNoteOn() || m.isNoteOff())) return;
    BoxEvent e;
    e.a = m.getNoteNumber();
    e.b = m.getVelocity();
    switch (page.load())
    {
        case Drums: if (m.isNoteOff()) return; e.type = BoxEvent::DrumHit; break;
        case Arp:   e.type = m.isNoteOn() ? BoxEvent::ArpOn : BoxEvent::ArpOff; break;
        default:    e.type = m.isNoteOn() ? BoxEvent::BassOn : BoxEvent::BassOff; break;
    }
    audio.post (e);
}

//==============================================================================
juce::File BoxController::presetFile (int instrument, int presetSlot) const
{
    return dataDir.getChildFile ("Presets").getChildFile (instrumentFolder (instrument))
                  .getChildFile (juce::String (presetSlot + 1).paddedLeft ('0', 2) + ".json");
}

bool BoxController::presetExists (int instrument, int presetSlot) const
{
    return presetFile (instrument, presetSlot).existsAsFile();
}

namespace
{
bool writeSafely (const juce::File& f, const juce::String& text)
{
    f.getParentDirectory().createDirectory();
    juce::TemporaryFile tmp (f);
    if (! tmp.getFile().replaceWithText (text)) return false;
    return tmp.overwriteTargetFileWithTemporary();   // never a half-written file, even on a power cut
}
} // namespace

bool BoxController::savePreset (int instrument, int presetSlot)
{
    auto state = data.inst[(size_t) instrument];
    state.tempo = data.globals[GTempo];
    const bool ok = writeSafely (presetFile (instrument, presetSlot),
                                 juce::JSON::toString (BoxData::instrumentToVar (instrument, state)));
    if (ok) data.loadedPreset[(size_t) instrument] = presetSlot;
    setFlash (ok ? sk ("Uložené: ") + instrumentName (instrument) + " " + juce::String (presetSlot + 1)
                 : sk ("Preset sa nepodarilo uložiť"));
    markDirty();
    return ok;
}

bool BoxController::loadPreset (int instrument, int presetSlot)
{
    const auto f = presetFile (instrument, presetSlot);
    InstrumentState state;
    if (! f.existsAsFile() || ! BoxData::instrumentFromVar (instrument, juce::JSON::parse (f), state))
    {
        setFlash (sk ("Pad ") + juce::String (presetSlot + 1) + sk (" je prázdny (Shift + Rec + pad = uložiť)"));
        return false;
    }
    data.inst[(size_t) instrument] = state;
    if (! audio.isPlaying()) data.globals[GTempo] = state.tempo;   // the tempo jumps only while stopped
    data.loadedPreset[(size_t) instrument] = presetSlot;
    if (instrument == Drums) regenerateDrums();
    if (instrument == Bass) regenerateBass();
    publish();
    markDirty();
    setFlash (sk ("Načítané: ") + instrumentName (instrument) + " " + juce::String (presetSlot + 1));
    return true;
}

void BoxController::setMuted (int instrument, bool m)
{
    data.muted[(size_t) juce::jlimit (0, NumInstruments - 1, instrument)] = m;
    publish();
    markDirty();
}

void BoxController::saveState()
{
    writeSafely (dataDir.getChildFile ("last.json"), juce::JSON::toString (data.toVar()));
    dirty = false;
}

void BoxController::timerCallback()
{
    bool got = false;
    DrumUserHit h;
    while (audio.popRecordedHit (h))
    {
        auto& hits = data.inst[Drums].drumHits;
        if (std::find (hits.begin(), hits.end(), h) == hits.end() && hits.size() < 256)
        {
            hits.push_back (h);
            got = true;
        }
    }
    if (got) { publish(); markDirty(); }

    if (dirty && juce::Time::getMillisecondCounter() - lastChange > 1500) saveState();
}

} // namespace catbox
