#include "CryptSynth.h"

namespace tomb
{
juce::StringArray chordNames() { return { "Single", "Minor", "Diminished", "Power", "Phrygian", "Octaves", "Sus" }; }
juce::StringArray arpNames() { return { "All at once", "Up", "Down", "Up & down", "Random" }; }
juce::StringArray arpRateNames() { return { "1/8", "1/16", "1/32", "1/64" }; }

std::vector<int> chordIntervals (Chord c)
{
    switch (c)
    {
        case Chord::Minor:      return { 0, 3, 7, 12 };
        case Chord::Diminished: return { 0, 3, 6, 9 };
        case Chord::Power:      return { 0, 7, 12 };
        case Chord::Phrygian:   return { 0, 1, 7, 12 };     // the minor second: the "something is wrong" sound
        case Chord::Octaves:    return { 0, 12, 24 };
        case Chord::Sus:        return { 0, 5, 7, 12 };
        case Chord::Off:        break;
    }
    return { 0 };
}

float envSeconds (float v, float most) noexcept { return 0.001f + std::pow (juce::jlimit (0.0f, 1.0f, v), 2.5f) * most; }

void Synth::prepare (double sampleRate) { sr = sampleRate; reset(); }

void Synth::reset()
{
    for (auto& v : voices) v = Voice();
    pedal = false;
}

int Synth::activeVoices() const noexcept
{
    int k = 0;
    for (auto& v : voices) if (v.active) ++k;
    return k;
}

void Synth::noteOn (int note, float velocity)
{
    ++clock;
    Voice* slot = nullptr;
    for (auto& v : voices) if (v.active && v.note == note) { slot = &v; break; }
    if (slot == nullptr) for (auto& v : voices) if (! v.active) { slot = &v; break; }
    if (slot == nullptr)
    {
        slot = &voices[0];
        for (auto& v : voices) if (v.age < slot->age) slot = &v;
    }
    auto& v = *slot;
    const bool retrigger = v.active;
    v.active = true; v.held = true; v.sustained = false; v.releasing = false;
    v.note = note;
    v.velocity = juce::jlimit (0.05f, 1.0f, velocity);
    v.time = 0.0;
    v.arpClock = 0.0; v.arpIndex = 0; v.arpDir = 1;
    v.age = clock;
    v.seed = clock * 2654435761u + (uint32_t) note;
    if (! retrigger) { v.env = 0.0f; for (auto& o : v.osc) o.phase = 0.0; v.detuned.phase = 0.25; }
}

void Synth::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.note == note && v.held)
        {
            v.held = false;
            if (pedal) v.sustained = true; else v.releasing = true;
        }
}

void Synth::sustain (bool down)
{
    pedal = down;
    if (! down) for (auto& v : voices) if (v.sustained) { v.sustained = false; v.releasing = true; }
}

void Synth::allNotesOff()
{
    for (auto& v : voices) if (v.active) { v.held = false; v.sustained = false; v.releasing = true; }
    pedal = false;
}

float Synth::renderVoice (Voice& v) noexcept
{
    const auto& s = settings;
    const double dt = 1.0 / sr;
    v.time += dt;

    // the envelope (attack, decay to sustain, release)
    const float a = envSeconds (s.attack, 2.0f), d = envSeconds (s.decay, 3.0f), r = envSeconds (s.release, 3.0f);
    if (v.releasing)
    {
        v.env -= (float) dt / r;
        if (v.env <= 0.0f) { v.env = 0.0f; v.active = false; return 0.0f; }
    }
    else if (v.time < a) v.env = juce::jmax (v.env, (float) (v.time / a));
    else
    {
        const float target = s.sustain;
        v.env += (target - v.env) * (float) (dt / (d * 0.35f));
        if (v.time < a + dt * 2.0) v.env = 1.0f;
    }
    float level = v.env;
    if (s.steps) level = std::floor (level * 15.0f + 0.5f) / 15.0f;      // 16 steps, like the chips
    if ((s.steps ? level <= 0.0f : v.env < 1.0e-4f) && ! v.releasing && s.sustain <= 0.001f && v.time > a) { v.active = false; return 0.0f; }

    // the pitch: the note, the sweep (falls from up to +2 octaves to the note), a late vibrato, the bend
    double semis = (double) (v.note + 12 * s.octave - 69) + (double) bend * 2.0;
    if (std::abs (s.sweep) > 0.001f) semis += (double) s.sweep * 24.0 * std::exp (-v.time / (0.02 + 0.25 * (1.0 - std::abs (s.sweep))));
    if (s.vibrato > 0.001f && v.time > 0.18) semis += (double) s.vibrato * 0.7 * std::sin (juce::MathConstants<double>::twoPi * 5.5 * (v.time - 0.18));

    const auto tones = chordIntervals (s.chord);
    const int count = (int) tones.size();
    float out = 0.0f;
    if (count > 1 && s.arp != Arp::Off)
    {
        // a fast arpeggio: one tone at a time
        const double div[] = { 2.0, 4.0, 8.0, 16.0 };
        const double stepSecs = 60.0 / juce::jlimit (40.0, 300.0, s.bpm) / div[juce::jlimit (0, 3, s.arpRate)];
        v.arpClock += dt;
        if (v.arpClock >= stepSecs)
        {
            v.arpClock -= stepSecs;
            switch (s.arp)
            {
                case Arp::Up:     v.arpIndex = (v.arpIndex + 1) % count; break;
                case Arp::Down:   v.arpIndex = (v.arpIndex - 1 + count) % count; break;
                case Arp::UpDown:
                    if (v.arpIndex + v.arpDir >= count || v.arpIndex + v.arpDir < 0) v.arpDir = -v.arpDir;
                    v.arpIndex += v.arpDir;
                    break;
                case Arp::Random: v.seed = v.seed * 1664525u + 1013904223u; v.arpIndex = (int) ((v.seed >> 16) % (uint32_t) count); break;
                case Arp::Off: break;
            }
        }
        const int k = s.arp == Arp::Down && v.time < stepSecs ? count - 1 : v.arpIndex;
        const double hz = 440.0 * std::exp2 ((semis + tones[(size_t) k]) / 12.0);
        out = v.osc[0].tick (s.wave, hz, sr);
        if (s.detune > 0.001f) out = 0.6f * out + 0.4f * v.detuned.tick (s.wave, hz * std::exp2 (s.detune * 0.4 / 12.0), sr);
    }
    else
    {
        for (int k = 0; k < juce::jmin (count, 4); ++k)
        {
            const double hz = 440.0 * std::exp2 ((semis + tones[(size_t) k]) / 12.0);
            out += v.osc[(size_t) k].tick (s.wave, hz, sr);
        }
        out /= std::sqrt ((float) count);
        if (s.detune > 0.001f)
            out = 0.6f * out + 0.4f * v.detuned.tick (s.wave, 440.0 * std::exp2 ((semis + s.detune * 0.4) / 12.0), sr);
    }
    if (s.sub > 0.001f)
        out += s.sub * 0.8f * v.osc[4].tick (Wave::Triangle, 440.0 * std::exp2 ((semis - 12.0) / 12.0), sr);

    return out * level * (0.3f + 0.7f * v.velocity) * 0.28f;
}

void Synth::render (float* l, float* r, int n)
{
    for (auto& v : voices)
    {
        if (! v.active) continue;
        for (int i = 0; i < n && v.active; ++i)
        {
            const float x = renderVoice (v);
            l[i] += x;
            r[i] += x;
        }
    }
}
} // namespace tomb
