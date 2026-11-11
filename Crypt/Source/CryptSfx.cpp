#include "CryptSfx.h"

namespace tomb
{
juce::StringArray kindNames()
{
    return { "Sword", "Hit", "Spell", "Fire", "Door", "Trap", "Death", "Chest", "Step", "Pickup", "Growl", "Ghost", "Level up", "Bell" };
}

juce::String kindFileName (Kind k) { return kindNames()[(int) k].toLowerCase().replace (" ", "_"); }

namespace
{
const std::vector<std::vector<int>>& patterns()
{
    static const std::vector<std::vector<int>> p { { 0 }, { 0, 7 }, { 0, 3, 7, 12, 15, 19, 24 }, { 0, -1, -5, -6, -12, -13 }, { 0, 12 } };
    return p;
}
}

juce::var Sfx::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("kind", (int) kind); o->setProperty ("wave", (int) wave);
    o->setProperty ("freq", freq); o->setProperty ("slide", slide); o->setProperty ("accel", accel);
    o->setProperty ("vibDepth", vibDepth); o->setProperty ("vibRate", vibRate);
    o->setProperty ("duty", duty); o->setProperty ("dutySweep", dutySweep);
    o->setProperty ("attack", attack); o->setProperty ("sustain", sustain); o->setProperty ("punch", punch); o->setProperty ("decay", decay);
    o->setProperty ("noise", noise); o->setProperty ("lowpass", lowpass); o->setProperty ("lowpassSweep", lowpassSweep); o->setProperty ("highpass", highpass);
    o->setProperty ("arpPattern", arpPattern); o->setProperty ("arpSpeed", arpSpeed); o->setProperty ("repeat", repeat);
    o->setProperty ("bits", bits); o->setProperty ("echo", echo); o->setProperty ("room", room);
    o->setProperty ("seed", (int) seed); o->setProperty ("number", number);
    return juce::var (o);
}

Sfx Sfx::fromVar (const juce::var& v)
{
    Sfx s;
    auto f = [&v] (const char* id, float def) { return v.hasProperty (id) ? (float) (double) v[id] : def; };
    s.kind = (Kind) juce::jlimit (0, (int) Kind::count - 1, (int) f ("kind", 0.0f));
    s.wave = (Wave) juce::jlimit (0, (int) Wave::Metal, (int) f ("wave", (float) (int) Wave::Noise));
    s.freq = juce::jlimit (20.0f, 8000.0f, f ("freq", 440.0f));
    s.slide = f ("slide", 0.0f); s.accel = f ("accel", 0.0f);
    s.vibDepth = f ("vibDepth", 0.0f); s.vibRate = f ("vibRate", 6.0f);
    s.duty = f ("duty", 0.5f); s.dutySweep = f ("dutySweep", 0.0f);
    s.attack = juce::jlimit (0.0f, 2.0f, f ("attack", 0.0f)); s.sustain = juce::jlimit (0.0f, 3.0f, f ("sustain", 0.1f));
    s.punch = f ("punch", 0.0f); s.decay = juce::jlimit (0.005f, 4.0f, f ("decay", 0.3f));
    s.noise = f ("noise", 0.0f); s.lowpass = f ("lowpass", 1.0f); s.lowpassSweep = f ("lowpassSweep", 0.0f); s.highpass = f ("highpass", 0.0f);
    s.arpPattern = juce::jlimit (0, 4, (int) f ("arpPattern", 0.0f)); s.arpSpeed = juce::jlimit (0.02f, 1.0f, f ("arpSpeed", 0.1f));
    s.repeat = juce::jlimit (0.0f, 40.0f, f ("repeat", 0.0f));
    s.bits = juce::jlimit (2.0f, 16.0f, f ("bits", 16.0f)); s.echo = f ("echo", 0.0f); s.room = f ("room", 0.0f);
    s.seed = (juce::uint32) (int) f ("seed", 1.0f); s.number = juce::jmax (1, (int) f ("number", 1.0f));
    return s;
}

Sfx generate (Kind kind, juce::uint32 seed, float darkness)
{
    juce::Random r ((juce::int64) seed * 7919 + (juce::int64) kind);
    auto between = [&r] (float a, float b) { return a + (b - a) * r.nextFloat(); };
    auto pick = [&r] (std::initializer_list<Wave> w) { return *(w.begin() + r.nextInt ((int) w.size())); };
    Sfx s;
    s.kind = kind;
    s.seed = seed;
    switch (kind)
    {
        case Kind::Sword:   // a swish of air, sometimes a ring of steel
            s.wave = r.nextFloat() < 0.6f ? Wave::Noise : Wave::Metal;
            s.freq = between (900.0f, 2400.0f); s.slide = between (-3.0f, -0.5f);
            s.sustain = between (0.01f, 0.05f); s.decay = between (0.12f, 0.3f); s.punch = between (0.2f, 0.6f);
            s.lowpass = between (0.6f, 0.9f); s.lowpassSweep = between (-1.5f, -0.3f); s.highpass = between (0.1f, 0.35f);
            s.room = between (0.0f, 0.25f);
            break;
        case Kind::Hit:     // a low thump falling fast
            s.wave = pick ({ Wave::Square, Wave::Pulse25, Wave::Triangle });
            s.freq = between (140.0f, 420.0f); s.slide = between (-8.0f, -3.0f);
            s.decay = between (0.08f, 0.22f); s.sustain = between (0.0f, 0.03f); s.punch = between (0.4f, 0.9f);
            s.noise = between (0.3f, 0.8f); s.lowpass = between (0.4f, 0.8f); s.bits = between (6.0f, 12.0f);
            break;
        case Kind::Spell:   // a shimmer that climbs, with an echo
            s.wave = pick ({ Wave::Triangle, Wave::Organ, Wave::Bells, Wave::Ghost, Wave::Pulse12 });
            s.freq = between (280.0f, 800.0f); s.slide = between (0.2f, 1.6f); s.accel = between (-1.0f, 0.5f);
            s.vibDepth = between (0.2f, 0.8f); s.vibRate = between (8.0f, 20.0f);
            s.attack = between (0.0f, 0.08f); s.sustain = between (0.15f, 0.4f); s.decay = between (0.4f, 0.9f);
            s.arpPattern = r.nextBool() ? 4 : 2; s.arpSpeed = between (0.04f, 0.09f);
            s.echo = between (0.3f, 0.6f); s.room = between (0.3f, 0.6f);
            break;
        case Kind::Fire:    // crackling, low noise
            s.wave = Wave::Noise;
            s.freq = between (60.0f, 220.0f); s.slide = between (-0.6f, 0.2f);
            s.attack = between (0.0f, 0.05f); s.sustain = between (0.2f, 0.5f); s.decay = between (0.3f, 0.7f); s.punch = between (0.3f, 0.7f);
            s.vibDepth = between (0.5f, 1.0f); s.vibRate = between (14.0f, 30.0f); s.lowpass = between (0.35f, 0.6f);
            s.room = between (0.1f, 0.3f);
            break;
        case Kind::Door:    // a slow creak, then the room
            s.wave = pick ({ Wave::Pulse12, Wave::Pulse25 });
            s.freq = between (70.0f, 150.0f); s.slide = between (-0.2f, 0.5f);
            s.vibDepth = between (0.6f, 1.8f); s.vibRate = between (18.0f, 34.0f);
            s.attack = between (0.02f, 0.1f); s.sustain = between (0.35f, 0.8f); s.decay = between (0.2f, 0.5f);
            s.lowpass = between (0.35f, 0.6f); s.noise = between (0.0f, 0.25f); s.room = between (0.4f, 0.7f);
            break;
        case Kind::Trap:    // a click and a metallic rattle
            s.wave = Wave::Metal;
            s.freq = between (900.0f, 2600.0f); s.slide = between (-2.0f, 0.0f);
            s.decay = between (0.2f, 0.45f); s.sustain = between (0.0f, 0.05f); s.punch = between (0.6f, 1.0f);
            s.repeat = between (10.0f, 24.0f); s.highpass = between (0.1f, 0.3f); s.room = between (0.1f, 0.35f);
            break;
        case Kind::Death:   // falling, wobbling, crushed
            s.wave = pick ({ Wave::Square, Wave::Pulse25, Wave::Ghost });
            s.freq = between (300.0f, 600.0f); s.slide = between (-1.4f, -0.4f);
            s.vibDepth = between (0.2f, 0.6f); s.vibRate = between (4.0f, 9.0f);
            s.sustain = between (0.2f, 0.5f); s.decay = between (0.8f, 1.6f);
            s.arpPattern = r.nextBool() ? 3 : 0; s.arpSpeed = between (0.08f, 0.16f);
            s.bits = between (4.0f, 8.0f); s.room = between (0.4f, 0.7f);
            break;
        case Kind::Chest:   // the old coin sound, a fifth up (or a sad minor third)
            s.wave = pick ({ Wave::Square, Wave::Pulse25, Wave::Pulse12 });
            s.freq = between (500.0f, 1100.0f);
            s.arpPattern = 1; s.arpSpeed = between (0.06f, 0.11f);
            s.sustain = between (0.05f, 0.12f); s.decay = between (0.25f, 0.5f); s.punch = between (0.2f, 0.5f);
            s.room = between (0.15f, 0.4f);
            break;
        case Kind::Step:    // a soft thud on stone
            s.wave = Wave::Noise;
            s.freq = between (90.0f, 240.0f); s.slide = between (-2.0f, 0.0f);
            s.decay = between (0.04f, 0.1f); s.punch = between (0.2f, 0.6f);
            s.lowpass = between (0.18f, 0.35f); s.room = between (0.05f, 0.25f);
            break;
        case Kind::Pickup:  // a little blip up
            s.wave = pick ({ Wave::Pulse25, Wave::Square, Wave::Triangle });
            s.freq = between (400.0f, 900.0f); s.slide = between (0.6f, 2.5f);
            s.decay = between (0.1f, 0.22f); s.sustain = between (0.02f, 0.06f);
            s.room = between (0.0f, 0.25f);
            break;
        case Kind::Growl:   // a low rough voice
            s.wave = pick ({ Wave::Pulse12, Wave::Square, Wave::Ghost });
            s.freq = between (45.0f, 95.0f); s.slide = between (-0.3f, 0.2f);
            s.vibDepth = between (0.3f, 0.9f); s.vibRate = between (22.0f, 38.0f);
            s.attack = between (0.03f, 0.12f); s.sustain = between (0.3f, 0.7f); s.decay = between (0.3f, 0.6f);
            s.noise = between (0.2f, 0.5f); s.lowpass = between (0.3f, 0.5f); s.bits = between (5.0f, 9.0f);
            s.room = between (0.2f, 0.5f);
            break;
        case Kind::Ghost:   // a long whisper with a wobble
            s.wave = pick ({ Wave::Ghost, Wave::Organ, Wave::Triangle });
            s.freq = between (180.0f, 420.0f); s.slide = between (-0.3f, 0.3f);
            s.vibDepth = between (0.6f, 1.6f); s.vibRate = between (2.0f, 5.0f);
            s.attack = between (0.2f, 0.5f); s.sustain = between (0.5f, 1.0f); s.decay = between (0.8f, 1.5f);
            s.noise = between (0.4f, 0.8f); s.lowpass = between (0.4f, 0.7f); s.room = between (0.7f, 1.0f); s.echo = between (0.2f, 0.5f);
            break;
        case Kind::LevelUp: // a minor run up
            s.wave = pick ({ Wave::Square, Wave::Pulse25, Wave::Organ });
            s.freq = between (220.0f, 440.0f);
            s.arpPattern = 2; s.arpSpeed = between (0.06f, 0.1f);
            s.sustain = between (0.4f, 0.6f); s.decay = between (0.3f, 0.6f);
            s.room = between (0.3f, 0.5f); s.echo = between (0.1f, 0.3f);
            break;
        case Kind::Bell:    // a church bell far away
            s.wave = Wave::Bells;
            s.freq = between (160.0f, 420.0f);
            s.vibDepth = between (0.0f, 0.12f); s.vibRate = between (3.0f, 6.0f);
            s.punch = between (0.3f, 0.6f); s.decay = between (1.4f, 2.6f);
            s.room = between (0.6f, 0.9f);
            break;
        case Kind::count: break;
    }
    // the darkness: lower, darker, more stone around it
    const float d = juce::jlimit (0.0f, 1.0f, darkness);
    s.freq *= std::exp2 (-d * 0.8f);
    s.lowpass *= 1.0f - 0.45f * d;
    s.room = juce::jlimit (0.0f, 1.0f, s.room + 0.35f * d);
    s.duty = between (0.1f, 0.5f);
    return s;
}

Sfx mutate (const Sfx& in, juce::uint32 seed)
{
    juce::Random r ((juce::int64) seed * 104729 + 17);
    auto nudge = [&r] (float v, float amount) { return v * (1.0f + (r.nextFloat() - 0.5f) * 2.0f * amount); };
    Sfx s = in;
    s.seed = seed;
    s.freq = nudge (s.freq, 0.15f);
    s.slide = nudge (s.slide, 0.3f) + (r.nextFloat() - 0.5f) * 0.2f;
    s.vibDepth = juce::jmax (0.0f, nudge (s.vibDepth, 0.3f));
    s.vibRate = nudge (s.vibRate, 0.2f);
    s.sustain = juce::jmax (0.0f, nudge (s.sustain, 0.25f));
    s.decay = juce::jmax (0.02f, nudge (s.decay, 0.25f));
    s.lowpass = juce::jlimit (0.05f, 1.0f, nudge (s.lowpass, 0.15f));
    s.noise = juce::jlimit (0.0f, 1.0f, s.noise + (r.nextFloat() - 0.5f) * 0.2f);
    s.duty = juce::jlimit (0.06f, 0.5f, nudge (s.duty, 0.3f));
    return s;
}

juce::AudioBuffer<float> render (const Sfx& s, double sr, float pitch, float length)
{
    const float stretch = juce::jlimit (0.25f, 4.0f, length);
    const float att = s.attack * stretch, sus = s.sustain * stretch, dec = s.decay * stretch;
    const float body = att + sus + dec;
    const float tail = (s.room > 0.01f ? 0.6f + 1.6f * s.room : 0.0f) + (s.echo > 0.01f ? 0.8f : 0.0f);
    const int n = juce::jmax (16, (int) ((body + tail) * sr));
    juce::AudioBuffer<float> out (1, n);
    out.clear();
    float* y = out.getWritePointer (0);

    ChipOsc osc;
    Lfsr noise;
    noise.reg = (uint16_t) (1 + (s.seed & 0x3fffu));
    float noiseHold = 0.0f;
    double noisePhase = 0.0;
    float lp = 0.0f, hpIn = 0.0f, hpOut = 0.0f;
    const auto& pat = patterns()[(size_t) juce::jlimit (0, 4, s.arpPattern)];
    const float pitchMul = std::exp2 (pitch / 12.0f);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) (i / sr);
        if (t > body) break;
        // the volume
        float env;
        if (t < att) env = t / juce::jmax (1.0e-4f, att);
        else if (t < att + sus) env = 1.0f + s.punch * (1.0f - (t - att) / juce::jmax (1.0e-4f, sus));
        else env = (1.0f - (t - att - sus) / juce::jmax (1.0e-4f, dec)) * (1.0f + (sus <= 0.0f ? s.punch * (1.0f - (t - att) / dec) : 0.0f));
        env = juce::jmax (0.0f, env);

        // the pitch (the slide starts again with `repeat`)
        const float tp = s.repeat > 0.0f ? std::fmod (t, 1.0f / s.repeat) : t;
        const float octaves = s.slide * tp / stretch + 0.5f * s.accel * tp * tp / (stretch * stretch);
        float semis = 12.0f * juce::jlimit (-8.0f, 8.0f, octaves);
        semis += pat[(size_t) juce::jmin ((int) pat.size() - 1, (int) (t / (s.arpSpeed * stretch)))];
        semis += s.vibDepth * 3.0f * std::sin (juce::MathConstants<float>::twoPi * s.vibRate * t) * juce::jmin (1.0f, t * 8.0f);
        const double hz = juce::jlimit (20.0, sr * 0.45, (double) (s.freq * pitchMul) * std::exp2 (semis / 12.0));
        const float duty = juce::jlimit (0.05f, 0.95f, s.duty + s.dutySweep * t);

        float x = osc.tick (s.wave, hz, sr, (s.wave == Wave::Pulse12 || s.wave == Wave::Pulse25 || s.wave == Wave::Square) ? duty : -1.0f);
        if (s.noise > 0.0f)
        {
            noisePhase += hz * 8.0 / sr;
            while (noisePhase >= 1.0) { noisePhase -= 1.0; noiseHold = noise.clock(); }
            x = x * (1.0f - s.noise) + noiseHold * s.noise;
        }
        // the filters: low-pass (it can sweep) and high-pass
        const float lpv = juce::jlimit (0.02f, 1.0f, s.lowpass + s.lowpassSweep * t / stretch);
        const float hzLp = 60.0f * std::pow (20000.0f / 60.0f, lpv);
        const float c = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hzLp / (float) sr);
        lp += (x - lp) * c;
        x = lpv >= 0.999f ? x : lp;
        if (s.highpass > 0.0f)
        {
            const float hzHp = 20.0f * std::pow (100.0f, s.highpass);
            const float k = std::exp (-juce::MathConstants<float>::twoPi * hzHp / (float) sr);
            hpOut = k * (hpOut + x - hpIn);
            hpIn = x;
            x = hpOut;
        }
        if (s.bits < 15.9f)
        {
            const float levels = std::exp2 (s.bits - 1.0f);
            x = std::round (x * levels) / levels;
        }
        y[i] = x * env * 0.5f;
    }

    // the room and the echo (the crypt), mono
    if (s.room > 0.01f || s.echo > 0.01f)
    {
        juce::AudioBuffer<float> right (1, n);
        right.copyFrom (0, 0, out, 0, 0, n);
        Space space;
        space.prepare (sr, 512);
        space.process (y, right.getWritePointer (0), n, 0.0f, 0.0f, s.room, s.echo, 140.0);
        out.addFrom (0, 0, right, 0, 0, n);
        out.applyGain (0.5f);
    }

    // trim the silence at the end, a short fade, and set the level to -1 dB
    const float peak = out.getMagnitude (0, 0, n);
    int last = n - 1;
    while (last > 16 && std::abs (y[last]) < peak * 0.0015f) --last;
    const int keep = juce::jmin (n, last + (int) (0.01 * sr));
    juce::AudioBuffer<float> trimmed (1, keep);
    trimmed.copyFrom (0, 0, out, 0, 0, keep);
    const int fade = juce::jmin (keep / 4, (int) (0.01 * sr));
    trimmed.applyGainRamp (0, keep - fade, fade, 1.0f, 0.0f);
    if (peak > 1.0e-6f) trimmed.applyGain (0.89f / peak);
    return trimmed;
}

bool saveWav (const juce::AudioBuffer<float>& b, double sr, const juce::File& f)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (f.createOutputStream());
    if (stream == nullptr) return false;
    juce::WavAudioFormat wav;
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (1).withBitsPerSample (16);
    auto writer = wav.createWriterFor (stream, opts);
    if (writer == nullptr) return false;
    return writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}
} // namespace tomb
