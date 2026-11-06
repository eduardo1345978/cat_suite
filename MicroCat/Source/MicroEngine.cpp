#include "MicroEngine.h"

namespace micro
{
void Engine::reset()
{
    channelNote.fill (-1);
    startOrder.fill (0);
    releaseOrder.fill (0);
    noteChannel.fill (-1);
    noteOut.fill (-1);
    counter = 0;
    for (auto& h : held) h = false;
    notesPlaying = 0;
    configPending = true;
    sentBend.fill (0.0);
    map.justRoot = -1;
    justRoot = -1;
}

void Engine::updateJustRoot() noexcept
{
    int low = 128;
    for (auto id : channelNote) if (id >= 0) low = juce::jmin (low, id % 128);
    map.justRoot = low < 128 ? low : -1;
    justRoot = map.justRoot;
}

void Engine::retune (juce::MidiBuffer& out, int pos, const Settings& s)
{
    // held notes follow the tuning as it moves (morph knob, live just intonation's anchor): a short glide
    const double a = 1.0 - std::exp (-juce::jmax (0.0005, s.blockSeconds) / 0.04);
    for (int i = 0; i < 15; ++i)
    {
        const int id = channelNote[(size_t) i];
        if (id < 0) continue;
        bool valid = true;
        const double target = keySemis (map, id % 128, valid) - noteOut[(size_t) id];
        if (! valid || ! std::isfinite (target)) continue;
        double& cur = sentBend[(size_t) i];
        if (std::abs (target - cur) < 1e-4) continue;
        const double next = std::abs (target - cur) < 0.002 ? target : cur + (target - cur) * a;
        const int before = bendValue (cur, s.bendRange), after = bendValue (next, s.bendRange);
        cur = next;
        if (after != before)
        {
            out.addEvent (juce::MidiMessage::pitchWheel (i + 2, after), pos);
            ++bendUpdates;
        }
    }
}

int Engine::bendValue (double semitones, double range) noexcept
{
    const double r = juce::jmax (0.5, range);
    return juce::jlimit (0, 16383, 8192 + (int) std::lround (semitones / r * 8192.0));
}

int Engine::allocate (int noteId, int voices, int& stolen) noexcept
{
    stolen = -1;
    const int n = juce::jlimit (1, 15, voices);
    int best = -1;
    // a free channel released longest ago (its release tail rings longest undisturbed)
    for (int i = 0; i < n; ++i)
        if (channelNote[(size_t) i] < 0 && (best < 0 || releaseOrder[(size_t) i] < releaseOrder[(size_t) best])) best = i;
    if (best < 0)
    {
        // all busy: the oldest note gives its channel up
        for (int i = 0; i < n; ++i)
            if (best < 0 || startOrder[(size_t) i] < startOrder[(size_t) best]) best = i;
        stolen = channelNote[(size_t) best];
        if (stolen >= 0) noteChannel[(size_t) stolen] = -1;
    }
    channelNote[(size_t) best] = noteId;
    startOrder[(size_t) best] = ++counter;
    noteChannel[(size_t) noteId] = (int8_t) (best + 2);
    return best + 2;
}

void Engine::release (int noteId) noexcept
{
    const int ch = noteChannel[(size_t) noteId];
    if (ch < 2) return;
    noteChannel[(size_t) noteId] = -1;
    const int idx = ch - 2;
    if (channelNote[(size_t) idx] == noteId)
    {
        channelNote[(size_t) idx] = -1;
        releaseOrder[(size_t) idx] = ++counter;
    }
}

void Engine::sendConfig (juce::MidiBuffer& out, int pos, const Settings& s)
{
    auto rpn = [&out, pos] (int ch, int number, int value)
    {
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 101, 0), pos);
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 100, number), pos);
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 6, value), pos);
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 38, 0), pos);
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 101, 127), pos);   // RPN null: nothing else changes it by accident
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 100, 127), pos);
    };
    const int voices = juce::jlimit (1, 15, s.voices);
    rpn (1, 6, voices);                                  // MPE Configuration Message: lower zone, master = 1
    rpn (1, 0, 2);                                       // the master channel's bend range (MPE default)
    for (int ch = 2; ch < 2 + voices; ++ch)
        rpn (ch, 0, juce::jlimit (1, 96, (int) std::lround (s.bendRange)));
    ++configsSent;
}

void Engine::panic (juce::MidiBuffer& out, int pos)
{
    for (int ch = 1; ch <= 16; ++ch)
    {
        out.addEvent (juce::MidiMessage::allNotesOff (ch), pos);
        out.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), pos);
    }
    const bool cfg = configPending;
    reset();
    configPending = cfg;
}

void Engine::process (const juce::MidiBuffer& in, juce::MidiBuffer& out, const Settings& s)
{
    // a new bend range, number of voices or bypass: start clean so no note stays on a stale channel
    if (! juce::approximatelyEqual (s.bendRange, lastRange) || s.voices != lastVoices || s.bypass != lastBypass)
    {
        if (lastVoices >= 0) panic (out, 0);
        lastRange = s.bendRange; lastVoices = s.voices; lastBypass = s.bypass;
        configPending = true;
    }
    map.morph = s.morph;
    map.just = s.just;
    map.rootKey = s.rootKey;
    map.rootHz = s.rootHz;
    if (configPending && ! s.bypass)
    {
        if (s.sendConfig) sendConfig (out, 0, s);
        configPending = false;
    }

    for (const auto meta : in)
    {
        const auto m = meta.getMessage();
        const int pos = meta.samplePosition;
        if (s.bypass)
        {
            if (m.isNoteOn()) held[(size_t) m.getNoteNumber()] = true;
            else if (m.isNoteOff()) held[(size_t) m.getNoteNumber()] = false;
            out.addEvent (m, pos);
            continue;
        }
        if (m.isNoteOn())
        {
            const int key = m.getNoteNumber();
            const int id = (m.getChannel() - 1) * 128 + key;
            if (s.just != JustOff)                             // the new note may be the chord's new lowest
            {
                int low = key;
                for (auto other : channelNote) if (other >= 0) low = juce::jmin (low, other % 128);
                map.justRoot = low;
                justRoot = low;
            }
            const auto kp = keyPitch (map, key);
            if (! kp.valid) continue;                          // above / below what MIDI can play
            if (noteChannel[(size_t) id] >= 2)                 // the same key again while held: end the old one
            {
                out.addEvent (juce::MidiMessage::noteOff (noteChannel[(size_t) id], noteOut[(size_t) id]), pos);
                release (id);
            }
            int stolen = -1;
            const int ch = allocate (id, s.voices, stolen);
            if (stolen >= 0) out.addEvent (juce::MidiMessage::noteOff (ch, noteOut[(size_t) stolen]), pos);
            out.addEvent (juce::MidiMessage::pitchWheel (ch, bendValue (kp.bend, s.bendRange)), pos);
            out.addEvent (juce::MidiMessage::noteOn (ch, kp.note, m.getVelocity()), pos);
            sentBend[(size_t) ch - 2] = kp.bend;
            noteOut[(size_t) id] = (int8_t) kp.note;
            held[(size_t) key] = true;
        }
        else if (m.isNoteOff())
        {
            const int key = m.getNoteNumber();
            const int id = (m.getChannel() - 1) * 128 + key;
            held[(size_t) key] = false;
            const int ch = noteChannel[(size_t) id];
            if (ch < 2) continue;
            out.addEvent (juce::MidiMessage::noteOff (ch, noteOut[(size_t) id], m.getVelocity()), pos);
            release (id);
            if (s.just != JustOff) updateJustRoot();          // the chord's anchor may move up
        }
        else if (m.isAftertouch())
        {
            const int id = (m.getChannel() - 1) * 128 + m.getNoteNumber();
            const int ch = noteChannel[(size_t) id];
            if (ch >= 2) out.addEvent (juce::MidiMessage::channelPressureChange (ch, m.getAfterTouchValue()), pos);
        }
        else if (m.isPitchWheel() || m.isController() || m.isChannelPressure() || m.isProgramChange())
        {
            auto copy = m;
            copy.setChannel (1);                               // the master channel: an MPE synth applies it to every note
            out.addEvent (copy, pos);
        }
        else out.addEvent (m, pos);
    }
    if (! s.bypass)
    {
        map.morph = s.morph;
        map.just = s.just;
        map.rootKey = s.rootKey;
        map.rootHz = s.rootHz;
        if (s.just == JustOff) { map.justRoot = -1; justRoot = -1; }
        else updateJustRoot();
        retune (out, juce::jmax (0, s.blockSize - 1), s);
    }
    int playing = 0;
    for (auto c : channelNote) playing += c >= 0 ? 1 : 0;
    notesPlaying = playing;
}
} // namespace micro
