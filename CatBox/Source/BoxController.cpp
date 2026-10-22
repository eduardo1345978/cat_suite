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
    lastBassNote = scaleNote (key.load(), scale.load(), 0, 36);
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

void BoxController::setLightScreen (bool light)
{
    data.lightScreen = light;
    markDirty();
}

int BoxController::numStepPages (int instrument) const
{
    const int len = instrument == Drums ? data.drumLength() : instrument == Bass ? data.bassLength() : 16;
    return juce::jmax (1, (len + numPads - 1) / numPads);
}

//==============================================================================
int BoxController::nextScaleNote (int note, int dir) const
{
    const auto& iv = scaleIntervals (scale.load());
    const int k = key.load();
    for (int n = note + dir; n >= 12 && n <= 96; n += dir)
        if (std::find (iv.begin(), iv.end(), ((n - k) % 12 + 12) % 12) != iv.end()) return n;
    return note;
}

void BoxController::editBassStep (int knob, int clicks)
{
    if (bassSel < 0 || bassSel >= maxSteps) return;
    auto& s = data.inst[Bass].bassSteps[(size_t) bassSel];
    if (s.note < 0) s.note = lastBassNote.load();
    if (knob == 1)
    {
        for (int i = 0; i < std::abs (clicks); ++i) s.note = nextScaleNote (s.note, clicks > 0 ? 1 : -1);
        lastBassNote = s.note;
    }
    else if (knob == 2) s.slide = clicks > 0;
    else if (knob == 3) s.accent = clicks > 0;
    publish();
    markDirty();
}

void BoxController::turnKnob (int knob, float clicks)
{
    if (knob < 0 || knob >= numKnobs || clicks == 0.0f) return;
    const int pg = page.load();
    const int s = knob + (shift.load() ? numKnobs : 0);

    // Bass steps: knobs 2-4 edit the selected step (note, slide, accent)
    const bool stepEdit = pg == Bass && stepView.load() && ! shift.load() && knob >= 1 && knob <= 3 && bassSel >= 0;
    const auto& d = paramDesc (pg, s);
    if (! stepEdit && ! d.used()) return;

    if (stepEdit || d.kind != Kind::Percent)
    {
        // whole clicks only (a mouse drag sends fractions)
        auto& rem = knobRemainder[(size_t) s];
        rem += clicks;
        const float whole = std::trunc (rem);
        if (whole == 0.0f) return;
        rem -= whole;
        if (stepEdit) { editBassStep (knob, (int) whole); return; }
        clicks = whole;
    }

    const float before = data.get (pg, s);
    data.set (pg, s, before + clicks * d.step() * (d.kind == Kind::Percent ? (d.max - d.min) : 1.0f));
    if (data.get (pg, s) == before) return;
    if (s == slot::Key || s == slot::Scale)
    {
        key = juce::roundToInt (data.globals[GKey]);
        scale = juce::roundToInt (data.globals[GScale]);
    }
    paramChanged();
}

void BoxController::paramChanged()
{
    publish();
    markDirty();
}

BoxController::KnobView BoxController::knobView (int knob) const
{
    KnobView v;
    const int pg = page.load();
    const int s = knob + (shift.load() ? numKnobs : 0);
    if (pg == Bass && stepView.load() && ! shift.load() && knob >= 1 && knob <= 3)
    {
        v.used = v.stepEdit = true;
        const auto st = bassSel >= 0 ? data.inst[Bass].bassSteps[(size_t) bassSel] : BassStep {};
        const juce::String stepName = bassSel >= 0 ? "Step " + juce::String (bassSel + 1) : juce::String ("Step");
        if (knob == 1) { v.name = stepName + " note"; v.value = bassSel >= 0 ? noteName (st.note) : "tap a step"; v.fraction = st.note >= 0 ? (st.note - 24) / 48.0f : 0.0f; }
        if (knob == 2) { v.name = "Slide"; v.value = bassSel >= 0 ? (st.slide ? "On" : "Off") : "-"; v.fraction = st.slide ? 1.0f : 0.0f; }
        if (knob == 3) { v.name = "Accent"; v.value = bassSel >= 0 ? (st.accent ? "On" : "Off") : "-"; v.fraction = st.accent ? 1.0f : 0.0f; }
        return v;
    }
    const auto& d = paramDesc (pg, s);
    if (! d.used()) return v;
    const float value = data.get (pg, s);
    v.used = true;
    v.global = d.global >= 0;
    v.name = d.perDrum ? drumPadName (data.selectedDrum) + (s == slot::DLevel ? " level" : " chance") : d.name;
    v.value = d.format (value);
    v.fraction = d.max > d.min ? (value - d.min) / (d.max - d.min) : 0.0f;
    return v;
}

void BoxController::publish()
{
    AudioSnapshot s;
    const auto& di = data.inst[Drums];
    const auto& d = di.values;
    const auto& a = data.inst[Arp].values;
    const auto& bi = data.inst[Bass];
    const auto& b = bi.values;

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
    s.fx[Bass].drive = b[slot::BDrive];
    s.fx[Bass].chorus = b[slot::BChorus];
    s.fx[Bass].delay = b[slot::BEcho];

    s.drumSteps = di.drumSteps;
    s.drumLevel = di.drumLevel;
    s.drumChance = di.drumChance;
    s.drumLength = data.drumLength();
    s.drumKit = juce::roundToInt (d[slot::DKit]);
    s.drumSwing = d[slot::DSwing];
    s.drumHuman = d[slot::DHuman];

    s.bassSteps = bi.bassSteps;
    s.bassLength = data.bassLength();
    s.bassOctave = juce::roundToInt (b[slot::BOctave]);
    s.bassSound = juce::roundToInt (b[slot::BSound]);
    s.bassSwing = b[slot::BSwing];
    s.acid.cutoff = b[slot::BCutoff];
    s.acid.resonance = b[slot::BResonance];
    s.acid.envMod = b[slot::BEnvMod];
    s.acid.decay = b[slot::BDecay];
    s.acid.accent = b[slot::BAccent];
    s.acid.slide = b[slot::BSlide];
    s.acid.wave = juce::roundToInt (b[slot::BWave]);
    if (s.bassSound != 0) s.fx[Bass].filter = 0.85f * (1.0f - b[slot::BCutoff]);   // the other bass sounds: cutoff = track filter

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

int BoxController::arpPadNote (int pad) const
{
    return scaleNote (key.load(), scale.load(), pad, 48 + 12 * padOct[Arp].load());
}

int BoxController::bassPadNote (int pad) const
{
    return scaleNote (key.load(), scale.load(), pad, 36 + 12 * padOct[Bass].load());
}

//==============================================================================
void BoxController::selectDrum (int pad)
{
    data.selectedDrum = juce::jlimit (0, numPads - 1, pad);
}

void BoxController::tapDrumStep (int step)
{
    if (step >= data.drumLength()) return;
    auto& v = data.inst[Drums].drumSteps[(size_t) data.selectedDrum][(size_t) step];
    v = v == 0 ? 100 : v < 120 ? 127 : 0;   // off -> on -> accent -> off
    publish();
    markDirty();
}

void BoxController::tapArpStep (int step)
{
    auto& st = data.inst[Arp].arpSteps[(size_t) juce::jlimit (0, 15, step)];
    st = (uint8_t) ((st + 1) % NumStepStates);
    publish();
    markDirty();
}

void BoxController::tapBassStep (int step)
{
    if (step >= data.bassLength()) return;
    auto& s = data.inst[Bass].bassSteps[(size_t) step];
    if (s.note < 0) { s.note = lastBassNote.load(); bassSel = step; }   // empty -> the last note you played
    else if (bassSel != step) bassSel = step;                          // filled -> pick it (knobs 2-4 edit it)
    else { s = {}; }                                                   // picked again -> rest
    publish();
    markDirty();
}

void BoxController::padDown (int pad, int velocity)
{
    if (pad < 0 || pad >= numPads) return;
    velocity = juce::jlimit (1, 127, velocity);
    padHeldInst[(size_t) pad] = -1;

    if (shift.load())
    {
        const bool save = rec.load() || saveArm.exchange (false);
        const int pg = page.load();
        runOnMessageThread ([this, save, pg, pad] { save ? savePreset (pg, pad) : loadPreset (pg, pad); });
        return;
    }

    const int pg = page.load();
    if (stepView.load())
    {
        const int step = stepPages[(size_t) pg].load() * numPads + pad;
        runOnMessageThread ([this, pg, step, pad]
        {
            if (pg == Drums) tapDrumStep (step);
            else if (pg == Arp) tapArpStep (pad);
            else tapBassStep (step);
        });
        return;
    }

    BoxEvent e;
    switch (pg)
    {
        case Drums:
            e.type = BoxEvent::DrumHit;
            e.a = drumPadNote (pad);
            e.b = velocity;
            e.c = pad;
            audio.post (e);
            runOnMessageThread ([this, pad] { selectDrum (pad); });
            break;
        case Arp:
            e.type = BoxEvent::ArpOn;
            e.a = arpPadNote (pad);
            e.b = velocity;
            padHeldInst[(size_t) pad] = Arp;
            padHeldNote[(size_t) pad] = e.a;
            audio.post (e);
            break;
        default:
            e.type = BoxEvent::BassOn;
            e.a = bassPadNote (pad);
            e.b = velocity;
            padHeldInst[(size_t) pad] = Bass;
            padHeldNote[(size_t) pad] = e.a;
            lastBassNote = e.a;
            audio.post (e);
            break;
    }
}

void BoxController::padUp (int pad)
{
    if (pad < 0 || pad >= numPads) return;
    const int inst = padHeldInst[(size_t) pad].exchange (-1);
    const int note = padHeldNote[(size_t) pad].exchange (-1);
    if (note < 0 || (inst != Arp && inst != Bass)) return;
    BoxEvent e;
    e.type = inst == Arp ? BoxEvent::ArpOff : BoxEvent::BassOff;
    e.a = note;
    audio.post (e);
}

//==============================================================================
juce::String BoxController::buttonLabel (int b) const
{
    const int pg = page.load();
    switch (b)
    {
        case BtnPlay:       return "Play";
        case BtnSteps:      return "Steps";
        case BtnShift:      return "Shift";
        case BtnTap:        return shift.load() ? "Clear" : "Tap";
        case BtnInstrument: return shift.load() ? "Mute" : "Instr";
        case BtnOctDown:    return pg == Drums ? (shift.load() ? "Page -" : "Drum -") : (stepView.load() && pg == Bass ? "Page -" : "Oct -");
        case BtnOctUp:      return pg == Drums ? (shift.load() ? "Page +" : "Drum +") : (stepView.load() && pg == Bass ? "Page +" : "Oct +");
        case BtnRec:        return shift.load() ? "Save" : pg == Arp ? "Hold" : "Rec";
        default:            return {};
    }
}

void BoxController::buttonDown (int b)
{
    const int pg = page.load();
    switch (b)
    {
        case BtnPlay:
            audio.setPlaying (! audio.isPlaying());
            break;

        case BtnSteps:
            stepView = ! stepView.load();
            if (stepView.load())
                setFlash (pg == Drums ? "Steps: pads = 16 steps of " + drumPadName (data.selectedDrum)
                        : pg == Arp   ? juce::String ("Steps: tap a pad: on > rest > tie > accent")
                                      : juce::String ("Steps: tap = set / pick, knobs 2-4 = note, slide, accent"));
            else
                setFlash ("Pads play");
            break;

        case BtnShift:
            shift = true;
            break;

        case BtnTap:
            if (shift.load())
            {
                if (pg == Drums)
                {
                    auto& steps = data.inst[Drums].drumSteps;
                    if (stepView.load()) { steps[(size_t) data.selectedDrum].fill (0); setFlash ("Cleared " + drumPadName (data.selectedDrum)); }
                    else { for (auto& p : steps) p.fill (0); setFlash ("Cleared all drums"); }
                }
                else if (pg == Bass)
                {
                    for (auto& s : data.inst[Bass].bassSteps) s = {};
                    bassSel = -1;
                    setFlash ("Cleared the bass line");
                }
                else
                {
                    BoxEvent e; e.type = BoxEvent::ArpClear;
                    audio.post (e);
                    setFlash ("Arp notes released");
                }
                publish();
                markDirty();
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
                setFlash (instrumentName (pg) + (m ? " muted" : " on"));
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
            const bool paging = (pg == Drums && shift.load()) || (pg == Bass && stepView.load());
            if (paging)
            {
                auto& sp = stepPages[(size_t) pg];
                sp = juce::jlimit (0, numStepPages (pg) - 1, sp.load() + dir);
                setFlash ("Steps " + juce::String (sp.load() * 16 + 1) + "-" + juce::String (sp.load() * 16 + 16));
            }
            else if (pg == Drums)
            {
                selectDrum ((data.selectedDrum + dir + numPads) % numPads);
                setFlash ("Drum: " + drumPadName (data.selectedDrum));
            }
            else
            {
                auto& o = data.padOctave[(size_t) pg];
                o = juce::jlimit (-2, 2, o + dir);
                padOct[(size_t) pg] = o;
                markDirty();
                setFlash ("Pad octave " + juce::String (o > 0 ? "+" : "") + juce::String (o));
            }
            break;
        }

        case BtnRec:
            rec = true;
            if (shift.load())
            {
                saveArm = true;   // the next pad saves (also when Rec is already let go, e.g. clicked with the mouse)
                setFlash ("Save: press a pad (1-16)");
                break;
            }
            if (pg == Arp)
            {
                latchOn = ! latchOn;
                BoxEvent e; e.type = BoxEvent::ArpLatch; e.a = latchOn ? 1 : 0;
                audio.post (e);
                setFlash (latchOn ? "Hold on: the arp keeps playing" : "Hold off");
            }
            else
            {
                audio.setRecording (! audio.isRecording());
                setFlash (audio.isRecording() ? (audio.isPlaying() ? "Rec: play the pads" : "Rec: press Play, then play the pads")
                                              : "Rec off");
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
        case BtnSteps: return stepView.load();
        case BtnShift: return shift.load();
        case BtnRec:   return page.load() == Arp ? latchOn : audio.isRecording();
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
        paramChanged();
        setFlash ("Tempo " + juce::String (juce::roundToInt (data.globals[GTempo])) + " BPM");
    }
    else setFlash ("Tap in time...");
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
    e.c = -1;
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
    const auto f = presetFile (instrument, presetSlot);
    return f.existsAsFile() && BoxData::instrumentFromVar (instrument, juce::JSON::parse (f), *std::make_unique<InstrumentState>());
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
    setFlash (ok ? "Saved: " + instrumentName (instrument) + " " + juce::String (presetSlot + 1) : juce::String ("Could not save the preset"));
    markDirty();
    return ok;
}

bool BoxController::loadPreset (int instrument, int presetSlot)
{
    const auto f = presetFile (instrument, presetSlot);
    InstrumentState state;
    if (! f.existsAsFile() || ! BoxData::instrumentFromVar (instrument, juce::JSON::parse (f), state))
    {
        setFlash ("Pad " + juce::String (presetSlot + 1) + " is empty (Shift + Rec + pad saves)");
        return false;
    }
    data.inst[(size_t) instrument] = state;
    if (! audio.isPlaying()) data.globals[GTempo] = state.tempo;   // the tempo jumps only while stopped
    data.loadedPreset[(size_t) instrument] = presetSlot;
    if (instrument == Bass) bassSel = -1;
    publish();
    markDirty();
    setFlash ("Loaded: " + instrumentName (instrument) + " " + juce::String (presetSlot + 1));
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
    RecordedNote r;
    while (audio.popRecorded (r))
    {
        if (r.instrument == Drums && r.value >= 0 && r.value < numPads && r.step >= 0 && r.step < maxSteps)
        {
            data.inst[Drums].drumSteps[(size_t) r.value][(size_t) r.step] = (uint8_t) juce::jlimit (1, 127, r.velocity);
            got = true;
        }
        else if (r.instrument == Bass && r.step >= 0 && r.step < maxSteps)
        {
            auto& steps = data.inst[Bass].bassSteps;
            steps[(size_t) r.step] = { r.value, false, r.velocity >= 110 };
            if (r.slideFrom >= 0 && r.slideFrom < maxSteps && r.slideFrom != r.step && steps[(size_t) r.slideFrom].note >= 0)
                steps[(size_t) r.slideFrom].slide = true;
            got = true;
        }
    }
    if (got) { publish(); markDirty(); }

    if (dirty && juce::Time::getMillisecondCounter() - lastChange > 1500) saveState();
}

} // namespace catbox
