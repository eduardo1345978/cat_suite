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
    rescanNames();
    publish();
    startTimerHz (30);
}

BoxController::~BoxController()
{
    stopTimer();
    if (dirty && ! isRemote()) saveState();
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

void BoxController::setPage (int p)
{
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "page"); o->setProperty ("p", p); sendRemote (o); return; }
    data.page = juce::jlimit (0, NumPages - 1, p);
    page = data.page;
    markDirty();
}

void BoxController::pickDrum (int pad)
{
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "drum"); o->setProperty ("p", pad); sendRemote (o); return; }
    selectDrum (pad);
    setFlash ("Drum: " + drumPadName (data.selectedDrum));
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
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "knob"); o->setProperty ("k", knob); o->setProperty ("c", clicks); sendRemote (o); return; }
    if (instrHeld)   // Instr held + any knob = flip through the pages quickly
    {
        instrRemainder += clicks;
        const int whole = (int) std::trunc (instrRemainder);
        if (whole == 0) return;
        instrRemainder -= (float) whole;
        instrTurned = true;
        setPage (((page.load() + whole) % NumPages + NumPages) % NumPages);
        setFlash (instrumentName (page.load()));
        return;
    }
    const int pg = page.load();
    const int s = knob + (shift.load() ? numKnobs : 0);
    lastKnobPg = pg;
    lastKnobSl = s;
    lastKnobAt = juce::Time::getMillisecondCounter();
    if (isSoundMode()) { editEngine (s, clicks); return; }
    if (pg == PageChords && isStepView() && ! shift.load() && knob >= 1 && knob <= 3)   // knobs 2-4 edit the picked chord
    {
        if (chordSel < 0) return;
        auto& rem = knobRemainder[(size_t) s];
        rem += clicks;
        const float w = std::trunc (rem);
        if (w == 0.0f) return;
        rem -= w;
        editChord (knob, (int) w);
        return;
    }

    // Bass steps: knobs 2-4 edit the picked step (note, slide, accent)
    const bool stepEdit = pg == Bass && isStepView() && ! shift.load() && knob >= 1 && knob <= 3 && bassSel >= 0;
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

void BoxController::resetKnob (int knob)
{
    if (knob < 0 || knob >= numKnobs) return;
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "reset"); o->setProperty ("k", knob); sendRemote (o); return; }
    const int pg = page.load();
    const int s = knob + (shift.load() ? numKnobs : 0);
    if (isSoundMode()) { resetEngineSlot (s); return; }
    if (pg == Bass && isStepView() && ! shift.load() && knob >= 1 && knob <= 3) return;   // a bass step has no default
    const auto& d = paramDesc (pg, s);
    if (! d.used()) return;
    lastKnobPg = pg;
    lastKnobSl = s;
    lastKnobAt = juce::Time::getMillisecondCounter();
    knobRemainder[(size_t) s] = 0.0f;
    const float before = data.get (pg, s);
    data.set (pg, s, d.perDrum ? 1.0f : d.def);
    if (data.get (pg, s) == before) return;
    if (s == slot::Key || s == slot::Scale)
    {
        key = juce::roundToInt (data.globals[GKey]);
        scale = juce::roundToInt (data.globals[GScale]);
    }
    paramChanged();
}

float BoxController::chaosHoldProgress() const
{
    if (! chaosArmed) return 0.0f;
    return juce::jlimit (0.0f, 1.0f, (float) (juce::Time::getMillisecondCounter() - chaosDownAt) / (float) chaosHoldMs);
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
    if (isSoundMode()) return engineView (knob + (shift.load() ? numKnobs : 0));
    if (pg == PageChords && isStepView() && ! shift.load() && knob >= 1 && knob <= 3)
    {
        v.used = v.stepEdit = true;
        if (chordSel < 0) { v.name = knob == 1 ? "Chord root" : knob == 2 ? "Chord type" : "Inversion"; v.value = "tap a pad"; return v; }
        const auto& ch = data.chords[(size_t) chordSel];
        const juce::String n = "Pad " + juce::String (chordSel + 1) + " ";
        if (knob == 1) { v.name = n + "root"; v.value = chordName (key.load(), { ch.root, 0, 0 }); v.fraction = ch.root / 23.0f; }
        if (knob == 2) { v.name = n + "type"; v.value = chordTypeNames()[ch.type]; v.fraction = ch.type / 15.0f; }
        if (knob == 3) { v.name = n + "inversion"; v.value = ch.inversion == 0 ? juce::String ("Root position") : juce::String (ch.inversion);
                         v.fraction = ch.inversion / 4.0f; }
        return v;
    }
    if (pg == Bass && isStepView() && ! shift.load() && knob >= 1 && knob <= 3)
    {
        v.used = v.stepEdit = true;
        const auto st = bassSel >= 0 ? data.inst[Bass].bassSteps[(size_t) bassSel] : BassStep {};
        const juce::String stepName = bassSel >= 0 ? "Step " + juce::String (bassSel + 1) : juce::String ("Step");
        if (knob == 1) { v.name = stepName + " note"; v.value = bassSel >= 0 ? noteName (st.note) : "tap a step"; v.fraction = st.note >= 0 ? (st.note - 24) / 48.0f : 0.0f; }
        if (knob == 2) { v.name = "Slide"; v.value = bassSel >= 0 ? (st.slide ? "On" : "Off") : "-"; v.fraction = st.slide ? 1.0f : 0.0f; }
        if (knob == 3) { v.name = "Accent"; v.value = bassSel >= 0 ? (st.accent ? "On" : "Off") : "-"; v.fraction = st.accent ? 1.0f : 0.0f; }
        return v;
    }
    return slotView (pg, s);
}

BoxController::KnobView BoxController::slotView (int pg, int s) const
{
    KnobView v;
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

void BoxController::publish (bool atBar)
{
    AudioSnapshot s;
    const auto& di = data.inst[Drums];
    const auto& d = di.values;
    const auto& a = data.inst[Arp].values;
    const auto& bi = data.inst[Bass];
    const auto& b = bi.values;
    const auto& f = data.fx;

    s.bpm = data.globals[GTempo];
    s.masterGain = data.globals[GMaster];
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
    s.bassFollow = juce::roundToInt (b[slot::BFollow]) == 1;
    s.acid.cutoff = b[slot::BCutoff];
    s.acid.resonance = b[slot::BResonance];
    s.acid.envMod = b[slot::BEnvMod];
    s.acid.decay = b[slot::BDecay];
    s.acid.accent = b[slot::BAccent];
    s.acid.slide = b[slot::BSlide];
    s.acid.wave = juce::roundToInt (b[slot::BWave]);
    if (s.bassSound != 0 && s.bassSound != bassSoundGlide()) s.fx[Bass].filter = 0.85f * (1.0f - b[slot::BCutoff]);   // the other bass sounds: cutoff = track filter
    if (s.bassSound == bassSoundGlide()) engine::toSettings (bi.glide, s.bassGlide, s.bassGlideFx);   // the other bass sounds: cutoff = track filter

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
    if (s.arpSound == arpSoundGlide) engine::toSettings (data.inst[Arp].glide, s.arpGlide, s.arpGlideFx);
    s.arpOn = juce::roundToInt (a[slot::AArpOn]) == 0;
    s.key = key.load();

    s.master.reverb = f[slot::FReverb];
    s.master.size = f[slot::FSize];
    s.master.delay = f[slot::FDelay];
    s.master.pump = f[slot::FPump];
    s.master.djFilter = f[slot::FDjFilter];
    s.master.tape = f[slot::FTape];
    s.master.vinyl = f[slot::FVinyl];
    s.master.crush = f[slot::FCrush];
    s.master.glue = f[slot::FGlue];
    s.master.width = f[slot::FWidth];
    s.master.pumpTarget = juce::roundToInt (f[slot::FPumpTarget]);
    s.master.sends = { f[slot::FSendDrums], f[slot::FSendArp], f[slot::FSendBass] };

    audio.setSnapshot (s, atBar);
}

int BoxController::arpPadNote (int pad) const
{
    if (juce::roundToInt (data.inst[Arp].values[slot::AKeys]) == LayoutPiano) return pianoPadNote (pad, 48 + 12 * padOct[Arp].load());
    return scaleNote (key.load(), scale.load(), pad, 48 + 12 * padOct[Arp].load());
}

int BoxController::bassPadNote (int pad) const
{
    if (juce::roundToInt (data.inst[Bass].values[slot::BKeys]) == LayoutPiano) return pianoPadNote (pad, 36 + 12 * padOct[Bass].load());
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
    if (isRemote())
    {
        runOnMessageThread ([this, pad, velocity] { auto* o = new juce::DynamicObject(); o->setProperty ("t", "pad"); o->setProperty ("p", pad); o->setProperty ("v", velocity); o->setProperty ("down", true); sendRemote (o); });
        return;
    }
    velocity = juce::jlimit (1, 127, velocity);
    padHeldInst[(size_t) pad] = -1;
    const int pg = page.load();

    // scenes: Scene mode, pad = recall (at the next bar while playing), Rec then pad = save
    if (sceneMode.load())
    {
        const bool save = rec.load() || saveArm.exchange (false);
        runOnMessageThread ([this, save, pad] { save ? saveScene (pad) : loadScene (pad); });
        return;
    }
    // presets of the page on screen: Shift + pad = load, Shift + Rec + pad = save
    if (shift.load())
    {
        const bool save = rec.load() || saveArm.exchange (false);
        runOnMessageThread ([this, save, pg, pad] { save ? savePreset (pg, pad) : loadPreset (pg, pad); });
        return;
    }

    if (pg == PageChords)   // chords on the Arp's sound
    {
        padHeldInst[(size_t) pad] = PageChords;
        padHeldNote[(size_t) pad] = pad;
        runOnMessageThread ([this, pad, velocity] { chordPadDown (pad, velocity); });
        return;
    }

    if (pg == PageFx)   // performance pads: on while held
    {
        BoxEvent e; e.type = BoxEvent::PerfOn; e.a = pad;
        padHeldInst[(size_t) pad] = PageFx;
        padHeldNote[(size_t) pad] = pad;
        audio.post (e);
        return;
    }

    if (isStepView())
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
            if (e.a < 0) return;   // a piano gap (no black key there)
            padHeldInst[(size_t) pad] = Arp;
            padHeldNote[(size_t) pad] = e.a;
            audio.post (e);
            break;
        default:
            e.type = BoxEvent::BassOn;
            e.a = bassPadNote (pad);
            e.b = velocity;
            if (e.a < 0) return;
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
    if (isRemote())
    {
        runOnMessageThread ([this, pad] { auto* o = new juce::DynamicObject(); o->setProperty ("t", "pad"); o->setProperty ("p", pad); o->setProperty ("down", false); sendRemote (o); });
        return;
    }
    const int inst = padHeldInst[(size_t) pad].exchange (-1);
    const int note = padHeldNote[(size_t) pad].exchange (-1);
    if (note < 0 || inst < 0) return;
    if (inst == PageChords) { runOnMessageThread ([this, pad] { chordPadUp (pad); }); return; }
    BoxEvent e;
    e.type = inst == PageFx ? BoxEvent::PerfOff : inst == Arp ? BoxEvent::ArpOff : BoxEvent::BassOff;
    e.a = note;
    if (inst == PageFx || inst == Arp || inst == Bass) audio.post (e);
}

//==============================================================================
juce::String BoxController::buttonLabel (int b) const
{
    const int pg = page.load();
    const bool sh = shift.load();
    switch (b)
    {
        case BtnPlay:       return sh ? "Tap" : "Play";
        case BtnSteps:      return sh ? "Clear" : "Steps";
        case BtnShift:      return "Shift";
        case BtnScene:      return "Scene";
        case BtnInstrument: return sh ? "Mute" : "Instr";
        case BtnOctDown:    return isSoundMode() ? "Page -" : pg == PageFx ? "-" : pg == Drums ? (sh ? "Page -" : "Drum -") : (isStepView() && pg == Bass ? "Page -" : "Oct -");
        case BtnOctUp:      return isSoundMode() ? "Page +" : pg == PageFx ? "-" : pg == Drums ? (sh ? "Page +" : "Drum +") : (isStepView() && pg == Bass ? "Page +" : "Oct +");
        case BtnChaos:      return sh ? "Undo" : "Chaos";
        case BtnSound:      return "Sound";
        case BtnRec:        return sh || sceneMode.load() ? "Save" : pg == Arp || pg == PageChords ? "Hold" : pg == PageFx ? "-" : "Rec";
        default:            return {};
    }
}

void BoxController::clearPage()
{
    const int pg = page.load();
    if (pg == Drums)
    {
        auto& steps = data.inst[Drums].drumSteps;
        if (isStepView()) { steps[(size_t) data.selectedDrum].fill (0); setFlash ("Cleared " + drumPadName (data.selectedDrum)); }
        else { for (auto& p : steps) p.fill (0); setFlash ("Cleared all drums"); }
    }
    else if (pg == Bass)
    {
        for (auto& s : data.inst[Bass].bassSteps) s = {};
        bassSel = -1;
        setFlash ("Cleared the bass line");
    }
    else if (pg == PageChords)
    {
        data.chords = defaultChords (scale.load());
        chordSel = -1;
        setFlash ("Chords: the key's chords again");
    }
    else if (pg == Arp)
    {
        BoxEvent e; e.type = BoxEvent::ArpClear;
        audio.post (e);
        setFlash ("Arp notes released");
    }
    else
    {
        data.fx = BoxData::fxDefaults();
        setFlash ("FX back to the start");
    }
    publish();
    markDirty();
}

void BoxController::buttonDown (int b)
{
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "button"); o->setProperty ("b", b); o->setProperty ("down", true); sendRemote (o); return; }
    const int pg = page.load();
    const bool sh = shift.load();
    switch (b)
    {
        case BtnPlay:
            if (sh) tapTempo();
            else audio.setPlaying (! audio.isPlaying());
            break;

        case BtnSteps:
            if (sh) { clearPage(); break; }
            if (pg == PageFx) { setFlash ("FX: hold a pad for an effect"); break; }
            stepViews[(size_t) pg] = ! stepViews[(size_t) pg].load();
            if (stepViews[(size_t) pg].load())
                setFlash (pg == Drums ? "Steps: pads = 16 steps of " + drumPadName (data.selectedDrum)
                        : pg == Arp   ? juce::String ("Steps: tap a pad: on > rest > tie > accent")
                        : pg == PageChords ? juce::String ("Edit chords: tap a pad, knobs 2-4 = root, type, inversion")
                                      : juce::String ("Steps: tap = set / pick, knobs 2-4 = note, slide, accent"));
            else
                setFlash ("Pads play");
            break;

        case BtnShift:
            shift = true;
            break;

        case BtnChaos:
            if (sh) { if (! undoChaos()) setFlash ("Nothing to take back"); }
            else { chaosArmed = true; chaosDownAt = juce::Time::getMillisecondCounter(); setFlash ("Keep holding Chaos..."); }
            break;

        case BtnSound:
            if (pg != Arp && pg != Bass) { setFlash ("Sound works on Arp and Bass"); break; }
            soundMode = ! soundMode.load();
            setFlash (soundMode.load() ? "Sound: " + engineTitle() + " - Oct - / Oct + = knob pages" : juce::String ("Sound off"));
            break;

        case BtnScene:
            sceneMode = ! sceneMode.load();
            saveArm = false;
            setFlash (sceneMode.load() ? "Scenes: pad = recall, Rec + pad = save" : "Scenes off");
            break;

        case BtnInstrument:
            if (sh)
            {
                if (pg >= NumInstruments) { setFlash ("Mute works on Drums, Arp and Bass"); break; }
                auto& m = data.muted[(size_t) pg];
                m = ! m;
                publish();
                markDirty();
                setFlash (instrumentName (pg) + (m ? " muted" : " on"));
            }
            else { instrHeld = true; instrTurned = false; instrRemainder = 0.0f; }   // switches on release, or by a knob while held
            break;

        case BtnOctDown:
        case BtnOctUp:
        {
            if (pg == PageFx) break;
            const int dir = b == BtnOctUp ? 1 : -1;
            if (isSoundMode())   // flip through the engine's knob pages
            {
                auto& ep = enginePages[(size_t) pg];
                ep = juce::jlimit (0, numEnginePages() - 1, ep.load() + dir);
                setFlash (enginePageName() + "  (" + juce::String (ep.load() + 1) + " / " + juce::String (numEnginePages()) + ")");
                markDirty();
                break;
            }
            if (pg == PageChords)
            {
                data.set (PageChords, slot::COctave, data.get (PageChords, slot::COctave) + (float) dir);
                const int o = juce::roundToInt (data.get (PageChords, slot::COctave));
                setFlash ("Chord octave " + juce::String (o > 0 ? "+" : "") + juce::String (o));
                markDirty();
                break;
            }
            const bool paging = (pg == Drums && sh) || (pg == Bass && isStepView());
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
            if (sh || sceneMode.load())
            {
                saveArm = true;   // the next pad saves (also when Rec is already let go, e.g. clicked with the mouse)
                setFlash (sceneMode.load() ? "Save scene: press a pad" : "Save preset: press a pad (1-16)");
                break;
            }
            if (pg == Arp || pg == PageChords)
            {
                latchOn = ! latchOn;
                BoxEvent e; e.type = BoxEvent::ArpLatch; e.a = latchOn ? 1 : 0;
                audio.post (e);
                setFlash (latchOn ? "Hold on: the arp keeps playing" : "Hold off");
            }
            else if (pg == PageFx)
            {
                setFlash ("FX: hold a pad for an effect");
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
    if (isRemote()) { auto* o = new juce::DynamicObject(); o->setProperty ("t", "button"); o->setProperty ("b", b); o->setProperty ("down", false); sendRemote (o); return; }
    if (b == BtnShift) { shift = false; if (! sceneMode.load()) saveArm = false; }
    if (b == BtnInstrument && instrHeld)
    {
        instrHeld = false;
        if (! instrTurned) setPage ((page.load() + 1) % NumPages);
    }
    if (b == BtnRec) rec = false;
    if (b == BtnChaos && chaosArmed) { chaosArmed = false; setFlash ("Chaos: hold it a moment"); }
}

bool BoxController::isButtonLit (int b) const
{
    switch (b)
    {
        case BtnPlay:  return audio.isPlaying();
        case BtnSteps: return isStepView();
        case BtnShift: return shift.load();
        case BtnInstrument: return instrHeld;
        case BtnScene: return sceneMode.load();
        case BtnSound: return isSoundMode();
        case BtnRec:   return page.load() == Arp || page.load() == PageChords ? latchOn : page.load() < NumInstruments && audio.isRecording();
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
                if (b == BtnRec && (shift.load() || sceneMode.load())) rec = down;
                runOnMessageThread ([this, b, down] { down ? buttonDown (b) : buttonUp (b); });
            }
        }
        return;
    }

    // a normal MIDI keyboard plays the instrument on screen
    if (! (m.isNoteOn() || m.isNoteOff())) return;
    if (page.load() == PageChords)   // a keyboard on the Chords page: each key plays a chord on that root (like the Orchid)
    {
        runOnMessageThread ([this, note = m.getNoteNumber(), vel = (int) m.getVelocity(), on = m.isNoteOn()]
        {
            const int k = numPads + note;
            if (! on) { releaseChord (k); return; }
            int shiftOct = 0;
            const auto c = chordForNote (note, shiftOct);
            playChord (k, c, shiftOct, vel);
        });
        return;
    }
    BoxEvent e;
    e.a = m.getNoteNumber();
    e.b = m.getVelocity();
    e.c = -1;
    switch (page.load())
    {
        case Drums: if (m.isNoteOff()) return; e.type = BoxEvent::DrumHit; break;
        case Bass:  e.type = m.isNoteOn() ? BoxEvent::BassOn : BoxEvent::BassOff; break;
        default:    e.type = m.isNoteOn() ? BoxEvent::ArpOn : BoxEvent::ArpOff; break;   // Arp and the FX page
    }
    audio.post (e);
}

//==============================================================================
juce::File BoxController::presetFile (int pg, int presetSlot) const
{
    return dataDir.getChildFile ("Presets").getChildFile (instrumentFolder (pg))
                  .getChildFile (juce::String (presetSlot + 1).paddedLeft ('0', 2) + ".json");
}

juce::File BoxController::sceneFile (int sceneSlot) const
{
    return dataDir.getChildFile ("Scenes").getChildFile (juce::String (sceneSlot + 1).paddedLeft ('0', 2) + ".json");
}

bool BoxController::presetExists (int pg, int presetSlot) const
{
    return presetName (pg, presetSlot).isNotEmpty();
}

bool BoxController::sceneExists (int sceneSlot) const
{
    return presetName (-1, sceneSlot).isNotEmpty();
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

bool BoxController::savePreset (int pg, int presetSlot)
{
    pg = juce::jlimit (0, NumPages - 1, pg);
    juce::var v;
    if (pg == PageFx) v = BoxData::fxToVar (data.fx);
    else if (pg == PageChords) v = BoxData::chordsToVar (data.chordVals, data.chords);
    else
    {
        auto state = data.inst[(size_t) pg];
        state.tempo = data.globals[GTempo];
        v = BoxData::instrumentToVar (pg, state);
    }
    const auto oldName = localPresetName (pg, presetSlot);
    v.getDynamicObject()->setProperty ("name", oldName.isNotEmpty() ? oldName : instrumentName (pg) + " " + juce::String (presetSlot + 1));
    const bool ok = writeSafely (presetFile (pg, presetSlot), juce::JSON::toString (v));
    if (ok) data.loadedPreset[(size_t) pg] = presetSlot;
    rescanNames();
    setFlash (ok ? "Saved: " + instrumentName (pg) + " " + juce::String (presetSlot + 1) : juce::String ("Could not save the preset"));
    markDirty();
    return ok;
}

bool BoxController::loadPreset (int pg, int presetSlot)
{
    pg = juce::jlimit (0, NumPages - 1, pg);
    if (isRemote())
    {
        auto* o = new juce::DynamicObject(); o->setProperty ("t", "load"); o->setProperty ("page", pg); o->setProperty ("slot", presetSlot);
        sendRemote (o);
        return presetExists (pg, presetSlot);
    }
    const auto f = presetFile (pg, presetSlot);
    bool ok = f.existsAsFile();
    if (ok)
    {
        const auto v = juce::JSON::parse (f);
        if (pg == PageFx) ok = BoxData::fxFromVar (v, data.fx);
        else if (pg == PageChords) { ok = BoxData::chordsFromVar (v, data.chordVals, data.chords); chordSel = -1; }
        else
        {
            InstrumentState state;
            ok = BoxData::instrumentFromVar (pg, v, state);
            if (ok)
            {
                data.inst[(size_t) pg] = state;
                if (! audio.isPlaying()) data.globals[GTempo] = state.tempo;   // the tempo jumps only while stopped
                if (pg == Bass) bassSel = -1;
            }
        }
    }
    if (! ok)
    {
        setFlash ("Pad " + juce::String (presetSlot + 1) + " is empty (Shift + Rec + pad saves)");
        return false;
    }
    data.loadedPreset[(size_t) pg] = presetSlot;
    publish();
    markDirty();
    setFlash ("Loaded: " + instrumentName (pg) + " " + juce::String (presetSlot + 1));
    return true;
}

bool BoxController::saveScene (int sceneSlot)
{
    auto v = data.sceneToVar();
    const auto oldName = localPresetName (-1, sceneSlot);
    v.getDynamicObject()->setProperty ("name", oldName.isNotEmpty() ? oldName : "Scene " + juce::String (sceneSlot + 1));
    const bool ok = writeSafely (sceneFile (sceneSlot), juce::JSON::toString (v));
    if (ok) data.loadedScene = sceneSlot;
    rescanNames();
    setFlash (ok ? "Saved scene " + juce::String (sceneSlot + 1) : juce::String ("Could not save the scene"));
    markDirty();
    return ok;
}

bool BoxController::loadScene (int sceneSlot)
{
    if (isRemote())
    {
        auto* o = new juce::DynamicObject(); o->setProperty ("t", "load"); o->setProperty ("page", -1); o->setProperty ("slot", sceneSlot);
        sendRemote (o);
        return sceneExists (sceneSlot);
    }
    const auto f = sceneFile (sceneSlot);
    if (! f.existsAsFile() || ! data.sceneFromVar (juce::JSON::parse (f)))
    {
        setFlash ("Scene " + juce::String (sceneSlot + 1) + " is empty (Rec + pad saves)");
        return false;
    }
    data.loadedScene = sceneSlot;
    for (auto& p : data.loadedPreset) p = -1;
    key = juce::roundToInt (data.globals[GKey]);
    scale = juce::roundToInt (data.globals[GScale]);
    bassSel = -1;
    publish (true);   // while playing it changes at the next bar
    markDirty();
    setFlash ("Scene " + juce::String (sceneSlot + 1) + (audio.isPlaying() ? " at the next bar" : ""));
    return true;
}

namespace
{
/** Which knobs Chaos may touch, and in which range (so it stays musical). */
struct ChaosRange { int slotIndex; float lo, hi; };

const std::vector<ChaosRange>& chaosRanges (int page)
{
    static const std::vector<ChaosRange> drums {
        { slot::DKit, 0, 3 }, { slot::DSwing, 0.0f, 0.5f }, { slot::DHuman, 0.0f, 0.6f }, { slot::DCrush, 0.0f, 0.45f },
        { slot::DDrive, 0.0f, 0.6f }, { slot::DFilter, 0.45f, 1.0f }, { slot::DEcho, 0.0f, 0.35f }, { slot::DChorus, 0.0f, 0.4f } };
    static const std::vector<ChaosRange> arp {
        { slot::ADivision, 1, 7 }, { slot::AMode, 0, NumArpModes - 1 }, { slot::AOctaves, 1, 3 }, { slot::AGate, 0.2f, 0.9f },
        { slot::ASwing, 0.0f, 0.4f }, { slot::ARatchet, 1, 2 }, { slot::AAccent, 0.2f, 0.9f }, { slot::AFilter, 0.4f, 1.0f },
        { slot::ASound, 0, 9 }, { slot::AEcho, 0.0f, 0.6f }, { slot::AChorus, 0.0f, 0.6f }, { slot::ADrive, 0.0f, 0.4f } };
    static const std::vector<ChaosRange> bass {
        { slot::BSound, 0, 4 }, { slot::BCutoff, 0.1f, 0.7f }, { slot::BResonance, 0.2f, 0.92f }, { slot::BEnvMod, 0.2f, 0.95f },
        { slot::BDecay, 0.1f, 0.8f }, { slot::BAccent, 0.3f, 1.0f }, { slot::BDrive, 0.0f, 0.7f }, { slot::BSlide, 0.1f, 0.7f },
        { slot::BWave, 0, 1 }, { slot::BEcho, 0.0f, 0.3f }, { slot::BChorus, 0.0f, 0.3f } };
    static const std::vector<ChaosRange> fx {
        { slot::FReverb, 0.2f, 0.9f }, { slot::FSize, 0.3f, 0.95f }, { slot::FDelay, 0.0f, 0.5f }, { slot::FPump, 0.0f, 0.7f },
        { slot::FDjFilter, 0.3f, 0.7f }, { slot::FTape, 0.0f, 0.6f }, { slot::FVinyl, 0.0f, 0.5f }, { slot::FCrush, 0.0f, 0.3f },
        { slot::FGlue, 0.0f, 0.6f }, { slot::FWidth, 0.3f, 0.9f }, { slot::FPumpTarget, 0, 3 },
        { slot::FSendDrums, 0.0f, 0.4f }, { slot::FSendArp, 0.1f, 0.7f }, { slot::FSendBass, 0.0f, 0.25f } };
    static const std::vector<ChaosRange> chords {
        { slot::CVoicing, 0, 3 }, { slot::CLeading, 0.3f, 1.0f }, { slot::CStrum, 0.0f, 0.5f }, { slot::CStrumDir, 0, 2 },
        { slot::CBassNote, 0, 1 }, { slot::CVelocity, 0.5f, 1.0f } };
    return page == Drums ? drums : page == Arp ? arp : page == Bass ? bass : page == PageChords ? chords : fx;
}
} // namespace

void BoxController::chaos()
{
    const int pg = page.load();
    auto& now = pg == PageFx ? data.fx : pg == PageChords ? data.chordVals : data.inst[(size_t) pg].values;
    chaosUndo[(size_t) pg] = now;
    hasChaosUndo[(size_t) pg] = true;
    for (const auto& r : chaosRanges (pg))
    {
        const auto& d = paramDesc (pg, r.slotIndex);
        float v = r.lo + chaosRnd.nextFloat() * (r.hi - r.lo);
        if (d.kind != Kind::Percent) v = (float) (juce::roundToInt (r.lo) + chaosRnd.nextInt (juce::roundToInt (r.hi) - juce::roundToInt (r.lo) + 1));
        now[(size_t) r.slotIndex] = juce::jlimit (d.min, d.max, v);
    }
    if (pg == Bass && chaosRnd.nextFloat() < 0.6f) now[slot::BSound] = 0.0f;   // mostly the Acid: it has the most knobs to play with
    data.loadedPreset[(size_t) pg] = -1;
    publish();
    markDirty();
    setFlash ("Chaos! (Shift + Chaos takes it back)");
}

bool BoxController::undoChaos()
{
    const int pg = page.load();
    if (! hasChaosUndo[(size_t) pg]) return false;
    auto& now = pg == PageFx ? data.fx : pg == PageChords ? data.chordVals : data.inst[(size_t) pg].values;
    std::swap (now, chaosUndo[(size_t) pg]);   // pressed again, it goes back to the chaos
    publish();
    markDirty();
    setFlash ("Back to before the chaos");
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

    if (chaosArmed && juce::Time::getMillisecondCounter() - chaosDownAt >= chaosHoldMs) { chaosArmed = false; chaos(); }

    if (dirty && ! isRemote() && juce::Time::getMillisecondCounter() - lastChange > 1500) saveState();
}

//==============================================================================
// names of presets and scenes
juce::String BoxController::localPresetName (int pg, int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= numPresetSlots) return {};
    return pg < 0 ? localSceneNames[(size_t) slotIndex] : localNames[(size_t) juce::jlimit (0, NumPages - 1, pg)][(size_t) slotIndex];
}

juce::String BoxController::presetName (int pg, int slotIndex) const
{
    if (! isRemote()) return localPresetName (pg, slotIndex);
    if (slotIndex < 0 || slotIndex >= numPresetSlots) return {};
    return pg < 0 ? remoteSceneNames[(size_t) slotIndex] : remoteNames[(size_t) juce::jlimit (0, NumPages - 1, pg)][(size_t) slotIndex];
}

juce::var BoxController::localPresetJson (int pg, int slotIndex) const
{
    const auto f = pg < 0 ? sceneFile (slotIndex) : presetFile (pg, slotIndex);
    return f.existsAsFile() ? juce::JSON::parse (f) : juce::var();
}

bool BoxController::storeLocalPresetJson (int pg, int slotIndex, const juce::var& preset)
{
    if (slotIndex < 0 || slotIndex >= numPresetSlots || ! preset.isObject()) return false;
    // only what this page can read
    bool valid = false;
    if (pg < 0) { BoxData probe; valid = probe.sceneFromVar (preset); }
    else if (pg == PageFx) { std::array<float, numSlots> tmp {}; valid = BoxData::fxFromVar (preset, tmp); }
    else if (pg == PageChords) { std::array<float, numSlots> tmp {}; std::array<ChordSlot, numPads> cs {}; valid = BoxData::chordsFromVar (preset, tmp, cs); }
    else { auto tmp = std::make_unique<InstrumentState>(); valid = BoxData::instrumentFromVar (pg, preset, *tmp); }
    if (! valid) return false;
    const bool ok = writeSafely (pg < 0 ? sceneFile (slotIndex) : presetFile (pg, slotIndex), juce::JSON::toString (preset));
    rescanNames();
    return ok;
}

bool BoxController::renamePreset (int pg, int slotIndex, const juce::String& newName)
{
    const auto name = newName.trim().substring (0, 24);
    if (name.isEmpty()) return false;
    if (isRemote())
    {
        auto* o = new juce::DynamicObject(); o->setProperty ("t", "rename"); o->setProperty ("page", pg); o->setProperty ("slot", slotIndex); o->setProperty ("name", name);
        sendRemote (o);
        return true;
    }
    auto v = localPresetJson (pg, slotIndex);
    if (! v.isObject()) return false;
    v.getDynamicObject()->setProperty ("name", name);
    const bool ok = writeSafely (pg < 0 ? sceneFile (slotIndex) : presetFile (pg, slotIndex), juce::JSON::toString (v));
    rescanNames();
    return ok;
}

void BoxController::rescanNames()
{
    for (int pg = -1; pg < NumPages; ++pg)
        for (int i = 0; i < numPresetSlots; ++i)
        {
            const auto f = pg < 0 ? sceneFile (i) : presetFile (pg, i);
            juce::String name;
            if (f.existsAsFile())
            {
                const auto v = juce::JSON::parse (f);
                bool valid = false;
                if (pg < 0) valid = v.isObject() && v["format"].toString() == "catbox-scene-1";
                else if (pg == PageFx) { std::array<float, numSlots> tmp {}; valid = BoxData::fxFromVar (v, tmp); }
                else if (pg == PageChords) { std::array<float, numSlots> tmp {}; std::array<ChordSlot, numPads> cs {}; valid = BoxData::chordsFromVar (v, tmp, cs); }
                else { auto tmp = std::make_unique<InstrumentState>(); valid = BoxData::instrumentFromVar (pg, v, *tmp); }
                if (valid)
                {
                    name = v["name"].toString().trim();
                    if (name.isEmpty()) name = (pg < 0 ? juce::String ("Scene") : instrumentName (pg)) + " " + juce::String (i + 1);
                }
            }
            (pg < 0 ? localSceneNames[(size_t) i] : localNames[(size_t) pg][(size_t) i]) = name;
        }
    if (onNamesChanged) onNamesChanged();
}

juce::var BoxController::namesVar() const
{
    juce::Array<juce::var> pages;
    for (int pg = 0; pg < NumPages; ++pg)
    {
        juce::Array<juce::var> names;
        for (auto& n : localNames[(size_t) pg]) names.add (n);
        pages.add (names);
    }
    juce::Array<juce::var> scenes;
    for (auto& n : localSceneNames) scenes.add (n);
    auto* o = new juce::DynamicObject();
    o->setProperty ("pages", pages);
    o->setProperty ("scenes", scenes);
    return juce::var (o);
}

//==============================================================================
// the link between the box and the PC app
void BoxController::sendRemote (juce::DynamicObject* o)
{
    juce::var v (o);
    if (remoteSink) remoteSink (v);
}

void BoxController::setRemoteSink (std::function<void (const juce::var&)> sink)
{
    const bool was = isRemote();
    remoteSink = std::move (sink);
    audio.setMirror (isRemote());
    if (was && ! isRemote())   // back to playing here: this PC's own state again
    {
        BoxData mine;
        const auto last = dataDir.getChildFile ("last.json");
        if (last.existsAsFile()) mine.fromVar (juce::JSON::parse (last));
        data = mine;
        shift = false; sceneMode = false; latchOn = false; bassSel = -1;
        for (auto& sv : stepViews) sv = false;
        audio.mirrorTransport (false, false);
        syncAtomics();
        rescanNames();
        publish();
    }
    if (onNamesChanged) onNamesChanged();
}

juce::var BoxController::makeState() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("t", "state");
    o->setProperty ("data", data.toVar());
    o->setProperty ("shift", shift.load());
    o->setProperty ("scene", sceneMode.load());
    o->setProperty ("latch", latchOn);
    o->setProperty ("bassSel", bassSel);
    juce::Array<juce::var> sv, sp;
    for (auto& x : stepViews) sv.add (x.load());
    for (auto& x : stepPages) sp.add (x.load());
    o->setProperty ("steps", sv);
    o->setProperty ("stepPages", sp);
    o->setProperty ("flash", getFlash());
    o->setProperty ("names", namesVar());
    o->setProperty ("knobPage", lastKnobPg);
    o->setProperty ("knobSlot", lastKnobSl);
    o->setProperty ("sound", soundMode.load());
    o->setProperty ("chordSel", chordSel);
    o->setProperty ("chordNow", chordNowName);
    juce::Array<juce::var> ep;
    for (auto& x : enginePages) ep.add (x.load());
    o->setProperty ("enginePages", ep);
    return juce::var (o);
}

juce::var BoxController::makeLive()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("t", "live");
    o->setProperty ("play", audio.isPlaying());
    o->setProperty ("rec", audio.isRecording());
    o->setProperty ("ds", audio.drumStep.load());
    o->setProperty ("bs", audio.bassStep.load());
    o->setProperty ("as", audio.arpStep.load());
    const uint64_t lo = audio.arpNotesLo.load(), hi = audio.arpNotesHi.load();
    o->setProperty ("an", juce::Array<juce::var> { (int) (lo & 0xffffffffu), (int) (lo >> 32), (int) (hi & 0xffffffffu), (int) (hi >> 32) });
    o->setProperty ("perf", (int) audio.perfHeld.load());
    o->setProperty ("flash", (int) audio.drumFlashLink.exchange (0));
    o->setProperty ("peak", audio.peak.load());
    o->setProperty ("wait", audio.sceneWaiting.load());
    o->setProperty ("bn", audio.bassNoteNow.load());
    // the last ~40 ms of sound, rounded (for the oscilloscope and spectrum on the PC)
    juce::Array<juce::var> wave;
    const int w = audio.spectrumWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < BoxAudio::spectrumSize; ++i)
        wave.add (std::round (audio.spectrumRing[(size_t) ((w + i) % BoxAudio::spectrumSize)].load (std::memory_order_relaxed) * 1000.0f) / 1000.0f);
    o->setProperty ("wave", wave);
    return juce::var (o);
}

void BoxController::applyState (const juce::var& v)
{
    if (! isRemote() || ! v.isObject()) return;
    data.fromVar (v["data"]);
    shift = (bool) v["shift"];
    sceneMode = (bool) v["scene"];
    latchOn = (bool) v["latch"];
    bassSel = (int) v.getProperty ("bassSel", -1);
    if (auto* sv = v["steps"].getArray())
        for (int i = 0; i < juce::jmin ((int) stepViews.size(), sv->size()); ++i) stepViews[(size_t) i] = (bool) (*sv)[i];
    if (auto* sp = v["stepPages"].getArray())
        for (int i = 0; i < juce::jmin ((int) stepPages.size(), sp->size()); ++i) stepPages[(size_t) i] = (int) (*sp)[i];
    const auto f = v["flash"].toString();
    if (f.isNotEmpty() && f != flash) setFlash (f);
    lastKnobPg = (int) v.getProperty ("knobPage", -1);
    lastKnobSl = (int) v.getProperty ("knobSlot", -1);
    soundMode = (bool) v.getProperty ("sound", false);
    chordSel = (int) v.getProperty ("chordSel", -1);
    chordNowName = v["chordNow"].toString();
    if (auto* ep = v["enginePages"].getArray())
        for (int i = 0; i < juce::jmin ((int) enginePages.size(), ep->size()); ++i) enginePages[(size_t) i] = (int) (*ep)[i];
    const auto names = v["names"];
    bool namesChanged = false;
    if (auto* pages = names["pages"].getArray())
        for (int pg = 0; pg < juce::jmin ((int) NumPages, pages->size()); ++pg)
            if (auto* list = (*pages)[pg].getArray())
                for (int i = 0; i < juce::jmin (numPresetSlots, list->size()); ++i)
                {
                    const auto n = (*list)[i].toString();
                    if (remoteNames[(size_t) pg][(size_t) i] != n) { remoteNames[(size_t) pg][(size_t) i] = n; namesChanged = true; }
                }
    if (auto* list = names["scenes"].getArray())
        for (int i = 0; i < juce::jmin (numPresetSlots, list->size()); ++i)
        {
            const auto n = (*list)[i].toString();
            if (remoteSceneNames[(size_t) i] != n) { remoteSceneNames[(size_t) i] = n; namesChanged = true; }
        }
    syncAtomics();
    if (namesChanged && onNamesChanged) onNamesChanged();
}

void BoxController::applyLive (const juce::var& v)
{
    if (! isRemote() || ! v.isObject()) return;
    audio.mirrorTransport ((bool) v["play"], (bool) v["rec"]);
    audio.drumStep = (int) v.getProperty ("ds", -1);
    audio.bassStep = (int) v.getProperty ("bs", -1);
    audio.arpStep = (int) v.getProperty ("as", -1);
    if (auto* an = v["an"].getArray(); an != nullptr && an->size() == 4)
    {
        auto u = [an] (int i) { return (uint64_t) (uint32_t) (int) (*an)[i]; };
        audio.arpNotesLo = u (0) | (u (1) << 32);
        audio.arpNotesHi = u (2) | (u (3) << 32);
    }
    audio.perfHeld = (uint32_t) (int) v.getProperty ("perf", 0);
    audio.drumFlash.fetch_or ((uint32_t) (int) v.getProperty ("flash", 0));
    audio.peak = (float) v.getProperty ("peak", 0.0);
    audio.sceneWaiting = (bool) v["wait"];
    audio.bassNoteNow = (int) v.getProperty ("bn", -1);
    if (auto* wave = v["wave"].getArray())
    {
        const int n = juce::jmin (BoxAudio::spectrumSize, wave->size());
        for (int i = 0; i < n; ++i) audio.spectrumRing[(size_t) i].store ((float) (*wave)[i], std::memory_order_relaxed);
        audio.spectrumWrite.store (n % BoxAudio::spectrumSize, std::memory_order_relaxed);
    }
}

void BoxController::handleLinkMessage (const juce::var& m, const std::function<void (const juce::var&)>& reply)
{
    if (isRemote() || ! m.isObject()) return;
    const auto t = m["t"].toString();
    const int pg = (int) m.getProperty ("page", 0), slotIndex = (int) m.getProperty ("slot", 0);
    if (t == "knob")        turnKnob ((int) m["k"], (float) m["c"]);
    else if (t == "reset")  resetKnob ((int) m["k"]);
    else if (t == "button") (bool) m["down"] ? buttonDown ((int) m["b"]) : buttonUp ((int) m["b"]);
    else if (t == "pad")    (bool) m["down"] ? padDown ((int) m["p"], (int) m.getProperty ("v", 100)) : padUp ((int) m["p"]);
    else if (t == "page")   setPage ((int) m["p"]);
    else if (t == "drum")   pickDrum ((int) m["p"]);
    else if (t == "load")   pg < 0 ? loadScene (slotIndex) : loadPreset (pg, slotIndex);
    else if (t == "rename") renamePreset (pg, slotIndex, m["name"].toString());
    else if (t == "putPreset")
    {
        const bool ok = storeLocalPresetJson (pg, slotIndex, m["data"]);
        setFlash (ok ? "From the PC: " + presetName (pg, slotIndex) : juce::String ("The PC sent a preset this page can't use"));
    }
    else if (t == "getPreset")
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("t", "preset");
        o->setProperty ("page", pg);
        o->setProperty ("slot", slotIndex);
        o->setProperty ("data", localPresetJson (pg, slotIndex));
        reply (juce::var (o));
    }
    else if (t == "hello")  reply (makeState());
}

//==============================================================================
// Sound mode: the page's engine
bool BoxController::isGlide (int pg) const
{
    if (pg == Arp) return juce::roundToInt (data.inst[Arp].values[slot::ASound]) == arpSoundGlide;
    if (pg == Bass) return juce::roundToInt (data.inst[Bass].values[slot::BSound]) == bassSoundGlide();
    return false;
}

int BoxController::numEnginePages() const
{
    return isGlide (page.load()) ? (int) engine::glidePages().size() : 1;
}

juce::String BoxController::enginePageName() const
{
    if (! isGlide (page.load())) return "Sound";
    const auto& pages = engine::glidePages();
    return pages[(size_t) juce::jlimit (0, (int) pages.size() - 1, enginePage())].name;
}

juce::String BoxController::engineTitle() const
{
    const int pg = page.load();
    if (pg != Arp && pg != Bass) return {};
    if (! isGlide (pg)) return paramDesc (pg, pg == Arp ? slot::ASound : slot::BSound).format (data.inst[(size_t) pg].values[pg == Arp ? slot::ASound : slot::BSound]);
    const auto& g = data.inst[(size_t) pg].glide;
    return "Glide: " + engine::glideSoundNames()[g.sound] + (g.edited ? " *" : "");
}

juce::String BoxController::engineId (int s) const
{
    const int pg = page.load();
    if ((pg != Arp && pg != Bass) || s < 0 || s >= numSlots) return {};
    if (! isGlide (pg)) return s == 0 ? juce::String ("@sound") : juce::String();
    const auto& pages = engine::glidePages();
    return pages[(size_t) juce::jlimit (0, (int) pages.size() - 1, enginePage())].ids[(size_t) s];
}

BoxController::KnobView BoxController::engineView (int s) const
{
    KnobView v;
    const int pg = page.load();
    const auto id = engineId (s);
    if (id.isEmpty()) return v;
    v.used = true;
    const int soundSlot = pg == Arp ? slot::ASound : slot::BSound;
    const auto& g = data.inst[(size_t) pg].glide;
    if (id == "@sound")
    {
        const auto& d = paramDesc (pg, soundSlot);
        const float x = data.inst[(size_t) pg].values[(size_t) soundSlot];
        v.name = "Engine";
        v.value = d.format (x);
        v.fraction = d.max > d.min ? (x - d.min) / (d.max - d.min) : 0.0f;
        return v;
    }
    if (id == "@preset")
    {
        const auto names = engine::glideSoundNames();
        v.name = "Preset";
        v.value = names[g.sound] + (g.edited ? " *" : "");
        v.fraction = names.size() > 1 ? (float) g.sound / (float) (names.size() - 1) : 0.0f;
        return v;
    }
    if (id == "@motion")
    {
        const auto names = engine::glideMotionNames();
        v.name = "Glide";
        v.value = names[g.motion];
        v.fraction = names.size() > 1 ? (float) g.motion / (float) (names.size() - 1) : 0.0f;
        return v;
    }
    if (id == "@init")
    {
        v.name = "Init";
        v.value = "turn right";
        return v;
    }
    const int i = glidecat::paramIndex (id);
    if (i < 0) { v.used = false; return v; }
    const auto& p = glidecat::params()[(size_t) i];
    v.name = engine::shortName (p);
    v.value = p.format (g.v[(size_t) i]);
    v.fraction = p.toNormalised (g.v[(size_t) i]);
    return v;
}

void BoxController::editEngine (int s, float clicks)
{
    const int pg = page.load();
    const auto id = engineId (s);
    if (id.isEmpty()) return;
    auto& inst = data.inst[(size_t) pg];
    auto& g = inst.glide;

    auto whole = [this, s, clicks]() -> int   // knobs that step: whole clicks only (a mouse drag sends fractions)
    {
        auto& rem = engineRemainder[(size_t) s];
        rem += clicks;
        const float w = std::trunc (rem);
        rem -= w;
        return (int) w;
    };

    if (id == "@sound")
    {
        const int w = whole();
        if (w == 0) return;
        const int soundSlot = pg == Arp ? slot::ASound : slot::BSound;
        data.set (pg, soundSlot, data.get (pg, soundSlot) + (float) w);
        enginePages[(size_t) pg] = 0;
        setFlash (engineTitle());
    }
    else if (id == "@preset")
    {
        const int w = whole();
        if (w == 0) return;
        engine::loadGlideSound (g, juce::jlimit (0, engine::glideSoundNames().size() - 1, g.sound + w));
        setFlash (engine::glideSoundNames()[g.sound]);
    }
    else if (id == "@motion")
    {
        const int w = whole();
        if (w == 0) return;
        engine::loadGlideMotion (g, juce::jlimit (0, engine::glideMotionNames().size() - 1, g.motion + w));
        setFlash ("Glide: " + engine::glideMotionNames()[g.motion]);
    }
    else if (id == "@init")
    {
        if (whole() <= 0) return;
        engine::initGlide (g);
        g.edited = true;
        setFlash ("Init: a plain sound to start from");
    }
    else
    {
        const int i = glidecat::paramIndex (id);
        if (i < 0) return;
        const auto& p = glidecat::params()[(size_t) i];
        const float before = g.v[(size_t) i];
        if (p.type == glidecat::ParamInfo::Float)
            engine::set (g, id, p.fromNormalised (p.toNormalised (before) + clicks * 0.01f));
        else
        {
            const int w = whole();
            if (w == 0) return;
            engine::set (g, id, before + (float) w);
        }
        if (g.v[(size_t) i] == before) return;
        g.edited = true;
    }
    paramChanged();
}

void BoxController::resetEngineSlot (int s)
{
    const auto id = engineId (s);
    const int i = glidecat::paramIndex (id);
    if (i < 0) return;
    auto& g = data.inst[(size_t) page.load()].glide;
    const float def = glidecat::params()[(size_t) i].def;
    if (g.v[(size_t) i] == def) return;
    engine::set (g, id, def);
    g.edited = true;
    lastKnobPg = page.load();
    lastKnobSl = s;
    lastKnobAt = juce::Time::getMillisecondCounter();
    paramChanged();
}

//==============================================================================
// The Chords page: chords on the pads (or Orchid style: one hand picks the type, the other the root),
// played on the Arp's sound - with the arp on they are arpeggiated, with it off they ring as chords.
ChordSlot BoxController::chordForPad (int pad) const
{
    if (! isOrchid()) return data.chords[(size_t) juce::jlimit (0, numPads - 1, pad)];
    const int d = juce::jlimit (0, 7, pad - 8);
    const int sc = scale.load(), k = key.load();
    ChordSlot c;
    if (juce::roundToInt (data.chordVals[slot::CRoots]) == 1)   // Piano: the root pads are the white keys C D E F G A B C
    {
        static const int white[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
        c.root = ((white[d] - k) % 12 + 12) % 12 + (d == 7 ? 12 : 0);   // the top C an octave up
        const auto& iv = scaleIntervals (sc);
        int degree = -1;
        for (size_t i = 0; i < iv.size(); ++i) if (iv[i] == c.root % 12) degree = (int) i;
        c.type = degree >= 0 ? diatonicChord (sc, degree, false).type : 0;
    }
    else c = diatonicChord (sc, d, false);   // Scale: the root pads are the key's scale, I .. VII and I
    if (orchidType.load() >= 0) c.type = orchidType.load();
    return c;
}

ChordSlot BoxController::chordForNote (int note, int& octaveShift) const
{
    const int k = key.load(), sc = scale.load();
    ChordSlot c;
    c.root = ((note - k) % 12 + 12) % 12;
    octaveShift = (int) std::floor ((double) (note - (48 + k + c.root)) / 12.0);
    const auto& iv = scaleIntervals (sc);
    int degree = -1;
    for (size_t i = 0; i < iv.size(); ++i) if (iv[i] == c.root) degree = (int) i;
    c.type = orchidType.load() >= 0 ? orchidType.load() : degree >= 0 ? diatonicChord (sc, degree, false).type : 0;
    return c;
}

int BoxController::buildChord (const ChordSlot& c, int octaveShift, std::array<int, 8>& out)
{
    const auto& iv = chordIntervals (c.type);
    const int n = juce::jmin (6, (int) iv.size());
    const int oct = juce::roundToInt (data.chordVals[slot::COctave]) + octaveShift;
    const int root = 48 + key.load() + c.root + 12 * oct;
    const int voicing = juce::roundToInt (data.chordVals[slot::CVoicing]);
    const float leading = data.chordVals[slot::CLeading];

    auto shape = [&] (int inversion, int shift, std::vector<int>& v)
    {
        v.clear();
        for (int i = 0; i < n; ++i) v.push_back (root + iv[(size_t) i] + shift);
        std::sort (v.begin(), v.end());
        for (int i = 0; i < inversion % n; ++i) { v.push_back (v.front() + 12); v.erase (v.begin()); }
        if (voicing == 1 && n >= 3) v[1] += 12;                                  // Open
        else if (voicing == 2 && n >= 4) v[(size_t) (n - 2)] -= 12;              // Drop 2
        else if (voicing == 3 && n >= 3) { v.front() -= 12; v.back() += 12; }   // Spread
        std::sort (v.begin(), v.end());
    };
    auto mean = [] (const std::vector<int>& v) { double s = 0; for (int x : v) s += x; return v.empty() ? 0.0 : s / (double) v.size(); };

    std::vector<int> best, cand;
    shape (c.inversion, 0, best);
    if (leading > 0.01f && lastChordCount > 0 && c.inversion == 0)   // voice leading: the shape that moves the least
    {
        const double home = mean (best);
        double bestCost = 1.0e9;
        std::vector<int> pick = best;
        for (int shift : { -12, 0, 12 })
            for (int inv = 0; inv < n; ++inv)
            {
                shape (inv, shift, cand);
                double move = 0.0;
                for (int x : cand)
                {
                    int d = 99;
                    for (int i = 0; i < lastChordCount; ++i) d = juce::jmin (d, std::abs (x - lastChord[(size_t) i]));
                    move += d;
                }
                const double cost = leading * move + (1.0 - leading) * 4.0 * std::abs (mean (cand) - home) + (shift != 0 ? 0.5 : 0.0);
                if (cost < bestCost) { bestCost = cost; pick = cand; }
            }
        best = pick;
    }
    int count = 0;
    for (int x : best) if (count < 7) out[(size_t) count++] = juce::jlimit (24, 108, x);
    lastChordCount = count;
    for (int i = 0; i < count; ++i) lastChord[(size_t) i] = out[(size_t) i];
    if (juce::roundToInt (data.chordVals[slot::CBassNote]) == 1 && count < 8)   // the root an octave below
        out[(size_t) count++] = juce::jlimit (24, 108, root - 12);
    return count;
}

void BoxController::playChord (int k, const ChordSlot& c, int octaveShift, int velocity)
{
    if (k < 0 || k >= numChordKeys) return;
    releaseChord (k);
    auto& notes = padChord[(size_t) k];
    const int count = buildChord (c, octaveShift, notes);
    padChordCount[(size_t) k] = count;

    const float dyn = data.chordVals[slot::CVelocity];
    const int vel = juce::jlimit (1, 127, juce::roundToInt ((float) velocity * (0.45f + 0.75f * dyn)));
    const float strumMs = data.chordVals[slot::CStrum] * 120.0f;   // between two notes
    const int dir = juce::roundToInt (data.chordVals[slot::CStrumDir]);
    std::array<int, 8> order {};
    for (int i = 0; i < count; ++i) order[(size_t) i] = notes[(size_t) i];
    std::sort (order.begin(), order.begin() + count);
    const bool down = dir == 1 || (dir == 2 && strumFlip);
    strumFlip = ! strumFlip;
    if (down) std::reverse (order.begin(), order.begin() + count);
    for (int i = 0; i < count; ++i)
    {
        BoxEvent e;
        e.type = BoxEvent::ArpOn;
        e.a = order[(size_t) i];
        e.b = vel;
        e.c = juce::roundToInt (strumMs * (float) i);
        audio.post (e);
    }

    juce::String names;
    static const char* nn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < count; ++i) names << nn[order[(size_t) i] % 12] << " ";
    chordNowName = chordName (key.load(), c) + "   " + names.trim();
}

void BoxController::releaseChord (int k)
{
    if (k < 0 || k >= numChordKeys) return;
    for (int i = 0; i < padChordCount[(size_t) k]; ++i)
    {
        BoxEvent e;
        e.type = BoxEvent::ArpOff;
        e.a = padChord[(size_t) k][(size_t) i];
        audio.post (e);
    }
    padChordCount[(size_t) k] = 0;
}

void BoxController::chordPadDown (int pad, int velocity)
{
    if (isStepView())   // edit: pick the pad's chord (and hear it)
    {
        chordSel = pad;
        playChord (pad, data.chords[(size_t) pad], 0, velocity);
        setFlash ("Pad " + juce::String (pad + 1) + ": " + chordName (key.load(), data.chords[(size_t) pad]));
        return;
    }
    if (! isOrchid()) { playChord (pad, data.chords[(size_t) pad], 0, velocity); return; }
    if (pad < 8)   // Orchid: the top row picks the chord type; a held root changes with it
    {
        orchidType = orchidPadType (pad);
        if (orchidRootPad >= 0) playChord (orchidRootPad, chordForPad (orchidRootPad), 0, orchidVelocity);
        return;
    }
    orchidRootPad = pad;
    orchidVelocity = velocity;
    playChord (pad, chordForPad (pad), 0, velocity);
}

void BoxController::chordPadUp (int pad)
{
    if (isOrchid() && pad < 8 && ! isStepView())
    {
        if (orchidType.load() == orchidPadType (pad)) orchidType = -1;
        return;
    }
    releaseChord (pad);
    if (orchidRootPad == pad) orchidRootPad = -1;
}

void BoxController::editChord (int knob, int clicks)
{
    if (chordSel < 0 || chordSel >= numPads) return;
    auto& ch = data.chords[(size_t) chordSel];
    if (knob == 1) ch.root = juce::jlimit (0, 23, ch.root + clicks);
    else if (knob == 2) ch.type = juce::jlimit (0, 15, ch.type + clicks);
    else if (knob == 3) ch.inversion = juce::jlimit (0, (int) chordIntervals (ch.type).size() - 1, ch.inversion + clicks);
    ch.inversion = juce::jlimit (0, (int) chordIntervals (ch.type).size() - 1, ch.inversion);
    data.loadedPreset[PageChords] = -1;
    setFlash ("Pad " + juce::String (chordSel + 1) + ": " + chordName (key.load(), ch));
    markDirty();
}

juce::String BoxController::chordPadLabel (int pad) const
{
    if (! isOrchid() || isStepView()) return chordName (key.load(), data.chords[(size_t) juce::jlimit (0, numPads - 1, pad)]);
    if (pad < 8)
    {
        static const char* t[] = { "Maj", "Min", "Dim", "Sus4", "7", "Maj7", "Min7", "Add9" };
        return t[pad];
    }
    return chordName (key.load(), chordForPad (pad));
}

} // namespace catbox
