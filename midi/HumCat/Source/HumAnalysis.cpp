#include "HumAnalysis.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace humcat
{
namespace
{
constexpr double analysisRate = 16000.0;
constexpr int hop = 160;            // 10 ms
constexpr int window = 400;         // 25 ms integration window
constexpr int tauMin = 14;          // ~1140 Hz
constexpr int tauMax = 260;         // ~62 Hz

const int majorScale[7] = { 0, 2, 4, 5, 7, 9, 11 };
const int minorScale[7] = { 0, 2, 3, 5, 7, 8, 10 };
const int* scaleOf (int scale) { return scale == 1 ? minorScale : majorScale; }

float median (std::vector<float> v)
{
    if (v.empty()) return 0.0f;
    std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
    return v[v.size() / 2];
}
} // namespace

juce::String keyName (int pc)
{
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[((pc % 12) + 12) % 12];
}

juce::StringArray styleNames() { return { "Pop", "Ballad", "Hip-Hop", "Rock", "Lo-Fi", "House" }; }
juce::StringArray partNames()  { return { "Chords", "Bass", "Lead", "Drums" }; }
int partChannel (int part)     { static const int ch[] = { 1, 2, 3, 10 }; return ch[juce::jlimit (0, NumParts - 1, part)]; }

//==============================================================================
PitchTrack trackPitch (const float* samples, int numSamples, double sampleRate, float sensitivity)
{
    PitchTrack out;
    out.hopSeconds = hop / analysisRate;
    if (samples == nullptr || numSamples <= 0 || sampleRate <= 0.0)
        return out;

    // low-pass and resample to 16 kHz
    std::vector<float> src (samples, samples + numSamples);
    {
        juce::IIRFilter lp1, lp2;
        const auto c = juce::IIRCoefficients::makeLowPass (sampleRate, juce::jmin (6500.0, sampleRate * 0.45));
        lp1.setCoefficients (c);
        lp2.setCoefficients (c);
        lp1.processSamples (src.data(), (int) src.size());
        lp2.processSamples (src.data(), (int) src.size());
    }
    const double ratio = sampleRate / analysisRate;
    const int n = (int) std::floor ((double) numSamples / ratio) - 4;
    if (n < window + tauMax + hop)
        return out;
    std::vector<float> x ((size_t) n);
    {
        juce::LagrangeInterpolator interp;
        interp.process (ratio, src.data(), x.data(), n);
    }

    const int frames = (n - window - tauMax) / hop;
    out.midi.assign ((size_t) frames, -1.0f);
    out.level.assign ((size_t) frames, 0.0f);
    std::vector<float> d ((size_t) tauMax + 1), cm ((size_t) tauMax + 1);
    std::vector<float> rawLevel ((size_t) frames, 0.0f), dip ((size_t) frames, 1.0f);

    const float threshold = 0.10f + 0.15f * juce::jlimit (0.0f, 1.0f, sensitivity);
    for (int f = 0; f < frames; ++f)
    {
        const float* fr = x.data() + f * hop;
        double energy = 0.0;
        for (int j = 0; j < window; ++j) energy += (double) fr[j] * fr[j];
        rawLevel[(size_t) f] = (float) std::sqrt (energy / window);

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            for (int j = 0; j < window; ++j)
            {
                const float diff = fr[j] - fr[j + tau];
                s += (double) diff * diff;
            }
            d[(size_t) tau] = (float) s;
        }
        double running = 0.0;
        cm[0] = 1.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(size_t) tau];
            cm[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (cm[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best < 0)
        {
            best = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (cm[(size_t) tau] < cm[(size_t) best]) best = tau;
            if (cm[(size_t) best] > threshold * 1.6f) { dip[(size_t) f] = 1.0f; continue; }
        }
        dip[(size_t) f] = cm[(size_t) best];

        // parabolic refinement
        double tauF = best;
        if (best > tauMin && best < tauMax - 1)
        {
            const double a = cm[(size_t) best - 1], b = cm[(size_t) best], c = cm[(size_t) best + 1];
            const double den = a - 2.0 * b + c;
            if (std::abs (den) > 1.0e-9) tauF = best + 0.5 * (a - c) / den;
        }
        const double hz = analysisRate / tauF;
        out.midi[(size_t) f] = (float) (69.0 + 12.0 * std::log2 (hz / 440.0));
    }

    // loudness gate: relative to the loudest singing
    float peak = 0.0f;
    for (auto l : rawLevel) peak = std::max (peak, l);
    const float gate = std::max (1.0e-4f, peak * juce::Decibels::decibelsToGain (-32.0f - 14.0f * sensitivity));
    for (int f = 0; f < frames; ++f)
    {
        out.level[(size_t) f] = peak > 0.0f ? rawLevel[(size_t) f] / peak : 0.0f;
        if (rawLevel[(size_t) f] < gate) out.midi[(size_t) f] = -1.0f;
    }

    // octave jumps of a single frame or two (YIN's typical error) and a 5-frame median
    auto m = out.midi;
    for (int f = 1; f + 1 < frames; ++f)
    {
        const float a = m[(size_t) f - 1], b = m[(size_t) f], c = m[(size_t) f + 1];
        if (a > 0 && b > 0 && c > 0)
        {
            if (std::abs (b - a - 12.0f) < 1.0f && std::abs (b - c - 12.0f) < 1.0f) m[(size_t) f] = b - 12.0f;
            else if (std::abs (a - b - 12.0f) < 1.0f && std::abs (c - b - 12.0f) < 1.0f) m[(size_t) f] = b + 12.0f;
        }
    }
    for (int f = 0; f < frames; ++f)
    {
        if (m[(size_t) f] < 0.0f) { out.midi[(size_t) f] = -1.0f; continue; }
        std::vector<float> win;
        for (int k = f - 2; k <= f + 2; ++k)
            if (k >= 0 && k < frames && m[(size_t) k] > 0.0f) win.push_back (m[(size_t) k]);
        out.midi[(size_t) f] = median (win);
    }
    // a lone voiced frame is noise
    for (int f = 1; f + 1 < frames; ++f)
        if (out.midi[(size_t) f] > 0.0f && out.midi[(size_t) f - 1] < 0.0f && out.midi[(size_t) f + 1] < 0.0f)
            out.midi[(size_t) f] = -1.0f;
    return out;
}

//==============================================================================
std::vector<Note> segment (const PitchTrack& track, float minNoteSeconds)
{
    std::vector<Note> notes;
    const int frames = (int) track.midi.size();
    const double hs = track.hopSeconds;
    float overallPeak = 0.0f;
    for (auto l : track.level) overallPeak = std::max (overallPeak, l);
    if (frames == 0 || overallPeak <= 0.0f) return notes;

    int start = -1, gap = 0, dev = 0, devStart = -1, dipFrames = 0;
    float notePeak = 0.0f;
    std::vector<float> pitches;

    auto close = [&] (int endFrame)
    {
        if (start >= 0 && endFrame > start && ! pitches.empty())
        {
            const int len = endFrame - start;
            // skip the scoop into the note when there is enough of it
            std::vector<float> body (pitches.begin() + (pitches.size() > 8 ? 3 : 0), pitches.end());
            Note nn;
            nn.start = start * hs;
            nn.length = len * hs;
            nn.pitch = (int) std::lround (median (body));
            double lv = 0.0;
            for (int k = start; k < endFrame; ++k) lv += track.level[(size_t) k];
            lv /= len;
            nn.velocity = (float) juce::jlimit (0.3, 1.0, 0.35 + 0.65 * std::sqrt (lv / overallPeak));
            if (nn.length >= minNoteSeconds) notes.push_back (nn);
        }
        start = -1; pitches.clear(); dev = 0; devStart = -1; dipFrames = 0; notePeak = 0.0f;
    };

    for (int f = 0; f < frames; ++f)
    {
        const float m = track.midi[(size_t) f];
        const float lv = track.level[(size_t) f];
        if (m < 0.0f)
        {
            if (start >= 0 && ++gap >= 3) close (f - gap + 1);
            continue;
        }
        if (start < 0) { start = f; gap = 0; pitches.push_back (m); notePeak = lv; continue; }
        gap = 0;

        // re-sung note on the same pitch: the voice dips and comes back
        if (lv < notePeak * 0.3f) ++dipFrames;
        else if (dipFrames >= 2 && lv > notePeak * 0.55f) { close (f - dipFrames); start = f; pitches.push_back (m); notePeak = lv; continue; }
        else dipFrames = 0;
        notePeak = std::max (notePeak, lv);

        std::vector<float> recent (pitches.end() - (long) std::min<size_t> (pitches.size(), 7), pitches.end());
        const float ref = median (recent);
        if (std::abs (m - ref) > 0.8f)
        {
            if (dev == 0) devStart = f;
            if (++dev >= 4)
            {
                close (devStart);
                start = devStart;
                for (int k = devStart; k <= f; ++k) if (track.midi[(size_t) k] > 0.0f) pitches.push_back (track.midi[(size_t) k]);
                notePeak = lv;
            }
            continue;
        }
        dev = 0;
        pitches.push_back (m);
    }
    close (frames);
    return notes;
}

//==============================================================================
void detectKey (const std::vector<Note>& notes, int& key, int& scale)
{
    static const double majorProfile[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minorProfile[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    std::array<double, 12> hist {};
    for (const auto& nn : notes) hist[(size_t) (((nn.pitch % 12) + 12) % 12)] += nn.length;
    // the last note weighs extra: songs tend to end at home
    if (! notes.empty()) hist[(size_t) (((notes.back().pitch % 12) + 12) % 12)] += notes.back().length * 0.5;

    auto corr = [&hist] (const double* prof, int k)
    {
        double mh = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mh += hist[(size_t) i]; mp += prof[i]; }
        mh /= 12; mp /= 12;
        double num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const double a = hist[(size_t) ((i + k) % 12)] - mh, b = prof[i] - mp;
            num += a * b; dh += a * a; dp += b * b;
        }
        return (dh > 0 && dp > 0) ? num / std::sqrt (dh * dp) : 0.0;
    };
    double best = -2.0;
    key = 0; scale = 0;
    for (int k = 0; k < 12; ++k)
        for (int s = 0; s < 2; ++s)
        {
            const double c = corr (s == 0 ? majorProfile : minorProfile, k);
            if (c > best + 1.0e-9) { best = c; key = k; scale = s; }
        }
}

juce::String Song::chordName (const ChordSlot& c) const
{
    return keyName (c.root) + (c.minor ? "m" : "");
}

//==============================================================================
namespace
{
struct Candidate { int degree; int root; bool minor; bool borrowedV; };

std::vector<Candidate> candidates (int key, int scale)
{
    std::vector<Candidate> out;
    const int* sc = scaleOf (scale);
    for (int d = 0; d < 7; ++d)
    {
        const int third = ((sc[(d + 2) % 7] - sc[d]) + 12) % 12;
        const int fifth = ((sc[(d + 4) % 7] - sc[d]) + 12) % 12;
        if (fifth != 7) continue;                       // no diminished chords
        out.push_back ({ d, (key + sc[d]) % 12, third == 3, false });
    }
    if (scale == 1) out.push_back ({ 4, (key + 7) % 12, false, true });   // the major V of harmonic minor
    return out;
}

bool chordTone (const Candidate& c, int pitch)
{
    const int pc = ((pitch - c.root) % 12 + 12) % 12;
    return pc == 0 || pc == (c.minor ? 3 : 4) || pc == 7;
}

double transition (int from, int to)
{
    if (from == to) return -0.25;
    static const int good[][2] = { { 0, 3 }, { 0, 4 }, { 0, 5 }, { 3, 4 }, { 3, 0 }, { 4, 0 }, { 4, 5 }, { 5, 3 }, { 1, 4 }, { 2, 5 }, { 5, 1 }, { 3, 1 }, { 5, 4 }, { 0, 1 } };
    static const double bonus[] = { 0.3, 0.3, 0.3, 0.4, 0.3, 0.6, 0.3, 0.4, 0.5, 0.3, 0.3, 0.15, 0.2, 0.15 };
    for (size_t i = 0; i < std::size (good); ++i)
        if (good[i][0] == from && good[i][1] == to) return bonus[i];
    return 0.0;
}

std::vector<int> voiceChord (int root, bool minor, bool seventh, int scale, int key, const std::vector<int>& previous)
{
    std::vector<int> pcs { root, (root + (minor ? 3 : 4)) % 12, (root + 7) % 12 };
    if (seventh)
    {
        // the diatonic 7th above the root
        const int* sc = scaleOf (scale);
        int seventhPc = (root + 10) % 12;
        for (int d = 0; d < 7; ++d)
            if ((key + sc[d]) % 12 == root) { seventhPc = (key + sc[(d + 6) % 7]) % 12; break; }
        pcs.push_back (seventhPc);
    }
    const double centre = previous.empty() ? 62.0
                                           : std::accumulate (previous.begin(), previous.end(), 0.0) / (double) previous.size();
    std::vector<int> best;
    double bestCost = 1.0e9;
    for (int inv = 0; inv < (int) pcs.size(); ++inv)
        for (int base = 48; base <= 64; ++base)
        {
            if (base % 12 != pcs[(size_t) inv]) continue;
            std::vector<int> v { base };
            for (int k = 1; k < (int) pcs.size(); ++k)
            {
                int p = pcs[(size_t) ((inv + k) % (int) pcs.size())];
                int note = v.back() + ((p - v.back()) % 12 + 12) % 12;
                if (note == v.back()) note += 12;
                v.push_back (note);
            }
            if (v.back() > 76) continue;
            const double c = std::abs (std::accumulate (v.begin(), v.end(), 0.0) / (double) v.size() - centre);
            if (c < bestCost) { bestCost = c; best = v; }
        }
    return best;
}

void addNote (std::vector<Note>& part, double start, double length, int pitch, float vel)
{
    part.push_back ({ start, length, pitch, vel });
}
} // namespace

Song makeSong (const std::vector<Note>& sung, const SongSettings& s)
{
    Song song;
    song.bpm = s.bpm > 0.0 ? s.bpm : 120.0;
    if (sung.empty()) { song.lengthBeats = 4.0; return song; }

    // seconds -> beats
    const double bps = song.bpm / 60.0;
    std::vector<Note> lead;
    for (const auto& nn : sung)
        lead.push_back ({ nn.start * bps, nn.length * bps, nn.pitch + 12 * s.leadOctave, nn.velocity });

    double shift = 0.0;
    if (s.trimStart) shift = lead.front().start;
    else
    {
        const double firstAbs = lead.front().start + s.startOffsetBeats;
        shift = -s.startOffsetBeats + std::floor (firstAbs / 4.0 + 1.0e-9) * 4.0;
    }
    for (auto& nn : lead) nn.start -= shift;

    // onto the grid
    if (s.grid > 0.0)
    {
        for (auto& nn : lead)
        {
            const double st = std::round (nn.start / s.grid) * s.grid;
            double en = std::round ((nn.start + nn.length) / s.grid) * s.grid;
            if (en <= st) en = st + s.grid;
            nn.start = st; nn.length = en - st;
        }
        std::vector<Note> clean;
        for (const auto& nn : lead)
        {
            if (! clean.empty() && std::abs (clean.back().start - nn.start) < 1.0e-6)
            {
                if (nn.length > clean.back().length) clean.back() = nn;
                continue;
            }
            clean.push_back (nn);
        }
        lead.swap (clean);
    }
    for (size_t i = 0; i + 1 < lead.size(); ++i)
        if (lead[i].start + lead[i].length > lead[i + 1].start)
            lead[i].length = std::max (s.grid > 0.0 ? s.grid : 0.05, lead[i + 1].start - lead[i].start);
    lead.erase (std::remove_if (lead.begin(), lead.end(), [] (const Note& nn) { return nn.start < -1.0e-6; }), lead.end());
    if (lead.empty()) { song.lengthBeats = 4.0; return song; }

    // key
    int key = 0, scale = 0;
    detectKey (lead, key, scale);
    if (s.key >= 0) key = s.key;
    if (s.scale >= 0) scale = s.scale;
    song.key = key; song.scale = scale;

    if (s.snapToScale)
    {
        const int* sc = scaleOf (scale);
        auto inScale = [&] (int p) { const int pc = ((p - key) % 12 + 12) % 12; for (int d = 0; d < 7; ++d) if (sc[d] == pc) return true; return false; };
        int previous = lead.front().pitch;
        for (auto& nn : lead)
        {
            if (! inScale (nn.pitch))
            {
                const bool up = inScale (nn.pitch + 1), down = inScale (nn.pitch - 1);
                if (up && down) nn.pitch += (previous > nn.pitch ? 1 : -1);
                else nn.pitch += up ? 1 : -1;
            }
            previous = nn.pitch;
        }
    }

    const auto& last = lead.back();
    song.lengthBeats = std::max (4.0, std::ceil ((last.start + last.length) / 4.0 - 1.0e-6) * 4.0);
    song.parts[Lead] = lead;

    // chords: Viterbi over the diatonic chords
    const double slotLen = s.chordBeats > 0.0 ? s.chordBeats : 4.0;
    const int slots = (int) std::ceil (song.lengthBeats / slotLen - 1.0e-6);
    const auto cands = candidates (key, scale);
    const int nc = (int) cands.size();
    std::vector<std::vector<double>> score ((size_t) slots, std::vector<double> ((size_t) nc, 0.0));
    std::vector<std::vector<int>> back ((size_t) slots, std::vector<int> ((size_t) nc, 0));
    std::vector<bool> empty ((size_t) slots, true);

    for (int sl = 0; sl < slots; ++sl)
    {
        const double a = sl * slotLen, b = a + slotLen;
        std::vector<double> emit ((size_t) nc, 0.0);
        for (const auto& nn : lead)
        {
            const double ov = std::min (b, nn.start + nn.length) - std::max (a, nn.start);
            if (ov <= 0.0) continue;
            empty[(size_t) sl] = false;
            const bool strong = std::abs (std::fmod (nn.start, 2.0)) < 1.0e-6 || std::abs (nn.start - a) < 1.0e-6;
            const double w = ov * (strong ? 1.5 : 1.0);
            for (int c = 0; c < nc; ++c)
            {
                const auto& cd = cands[(size_t) c];
                if (chordTone (cd, nn.pitch)) emit[(size_t) c] += w * (((nn.pitch - cd.root) % 12 + 12) % 12 == 0 ? 1.15 : 1.0);
                else emit[(size_t) c] -= w * 0.35;
            }
        }
        for (int c = 0; c < nc; ++c)
        {
            double e = emit[(size_t) c];
            if (cands[(size_t) c].borrowedV) e -= 0.1;
            if (sl == 0 && cands[(size_t) c].degree == 0) e += 0.5;
            if (sl == slots - 1 && cands[(size_t) c].degree == 0) e += 0.6;
            if (sl == 0) { score[0][(size_t) c] = e; continue; }
            double best = -1.0e9; int arg = 0;
            for (int p = 0; p < nc; ++p)
            {
                double t = score[(size_t) sl - 1][(size_t) p] + (empty[(size_t) sl] ? (p == c ? 0.2 : -0.1)
                                                                                     : transition (cands[(size_t) p].degree, cands[(size_t) c].degree));
                if (t > best) { best = t; arg = p; }
            }
            score[(size_t) sl][(size_t) c] = best + e;
            back[(size_t) sl][(size_t) c] = arg;
        }
    }
    std::vector<int> path ((size_t) slots, 0);
    {
        int arg = 0;
        for (int c = 1; c < nc; ++c) if (score[(size_t) slots - 1][(size_t) c] > score[(size_t) slots - 1][(size_t) arg]) arg = c;
        for (int sl = slots - 1; sl >= 0; --sl) { path[(size_t) sl] = arg; arg = back[(size_t) sl][(size_t) arg]; }
    }
    for (int sl = 0; sl < slots; ++sl)
    {
        const auto& cd = cands[(size_t) path[(size_t) sl]];
        song.chords.push_back ({ sl * slotLen, std::min (slotLen, song.lengthBeats - sl * slotLen), cd.root, cd.minor, cd.degree });
    }

    // chords, bass and drums in the chosen style
    const int style = juce::jlimit (0, NumStyles - 1, s.style);
    std::vector<int> voicing;
    for (const auto& ch : song.chords)
    {
        voicing = voiceChord (ch.root, ch.minor, style == LoFi, scale, key, voicing);
        auto& cp = song.parts[Chords];
        const double a = ch.start, L = ch.length;
        auto hit = [&] (double at, double len, float vel) { if (at < a + L - 1.0e-6) for (int p : voicing) addNote (cp, at, std::min (len, a + L - at), p, vel); };
        switch (style)
        {
            case Pop: case LoFi: hit (a, L, style == LoFi ? 0.55f : 0.68f); break;
            case Ballad:
            {
                std::vector<int> arp { voicing[0], voicing[std::min<size_t> (2, voicing.size() - 1)], voicing[1], voicing.back() + 12 - 12 * (voicing.back() > 72) };
                for (double t = 0.0, k = 0; t < L - 1.0e-6; t += 0.5, ++k)
                    addNote (cp, a + t, 0.48, arp[(size_t) ((int) k % (int) arp.size())], 0.5f + 0.1f * ((int) k % 4 == 0));
                break;
            }
            case HipHop: for (double bar = 0; bar < L - 1.0e-6; bar += 4.0) { hit (a + bar, 1.5, 0.66f); hit (a + bar + 2.5, 1.0, 0.58f); } break;
            case Rock: for (double t = 0; t < L - 1.0e-6; t += 0.5) hit (a + t, 0.45, std::fmod (t, 1.0) < 1.0e-6 ? 0.78f : 0.62f); break;
            case House: for (double t = 0.5; t < L - 1.0e-6; t += 1.0) hit (a + t, 0.3, 0.7f); break;
            default: break;
        }

        // bass: the root low down
        auto& bp = song.parts[Bass];
        const int root = 36 + ((ch.root - 36) % 12 + 12) % 12;
        const int fifth = root + 7 > 50 ? root - 5 : root + 7;
        auto bn = [&] (double at, double len, int p, float vel) { if (at < a + L - 1.0e-6) addNote (bp, a + at, std::min (len, L - at), p, vel); };
        for (double bar = 0; bar < L - 1.0e-6; bar += 4.0)
            switch (style)
            {
                case Pop:    bn (bar, 1.0, root, 0.8f); bn (bar + 1.5, 0.5, root, 0.65f); bn (bar + 2.0, 1.0, root, 0.75f); bn (bar + 3.5, 0.5, fifth, 0.6f); break;
                case Ballad: bn (bar, 2.0, root, 0.7f); bn (bar + 2.0, 2.0, fifth, 0.6f); break;
                case HipHop: bn (bar, 1.25, root, 0.85f); bn (bar + 1.75, 0.25, root + 12, 0.6f); bn (bar + 2.5, 1.0, root, 0.75f); break;
                case Rock:   for (double t = 0; t < 4.0; t += 0.5) bn (bar + t, 0.45, root, std::fmod (t, 1.0) < 1.0e-6 ? 0.8f : 0.65f); break;
                case LoFi:   bn (bar, 1.5, root, 0.7f); bn (bar + 2.5, 1.0, fifth, 0.6f); break;
                case House:  for (double t = 0.5; t < 4.0; t += 1.0) bn (bar + t, 0.4, root, 0.75f); break;
                default: break;
            }
    }

    // drums: one bar patterns of 16 steps
    struct Pattern { const char* kick; const char* snare; const char* hat; const char* open; int snareNote; };
    static const Pattern patterns[NumStyles] = {
        { "x......xx.......", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", 38 },   // Pop
        { "x.........x.....", "........x.......", "x...x...x...x...", "................", 37 },   // Ballad (rim)
        { "x......x..x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", 38 },   // Hip-Hop
        { "x.......x.x.....", "....x.......x...", "x.x.x.x.x.x.x...", "..............x.", 38 },   // Rock
        { "x.........x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", 37 },   // Lo-Fi (rim)
        { "x...x...x...x...", "....x.......x...", "................", "..x...x...x...x.", 39 } }; // House (clap)
    const auto& pat = patterns[style];
    auto& dp = song.parts[Drums];
    const int bars = (int) std::round (song.lengthBeats / 4.0);
    for (int bar = 0; bar < bars; ++bar)
    {
        const double b0 = bar * 4.0;
        const bool fill = bars > 1 && bar == bars - 1;
        for (int st = 0; st < 16; ++st)
        {
            double at = b0 + st * 0.25;
            if (style == HipHop || style == LoFi) if (st % 2 == 1) at += 0.06;   // swing
            if (pat.kick[st] == 'x') addNote (dp, at, 0.25, 36, st == 0 ? 0.95f : 0.8f);
            if (pat.snare[st] == 'x' && ! (fill && st >= 12)) addNote (dp, at, 0.25, pat.snareNote, 0.85f);
            if (pat.hat[st] == 'x') addNote (dp, at, 0.2, style == Ballad ? 51 : 42, st % 4 == 0 ? 0.6f : 0.42f);
            if (pat.open[st] == 'x') addNote (dp, at, 0.25, 46, 0.55f);
            if (fill && st >= 12) addNote (dp, at, 0.2, 38, 0.6f + 0.08f * (float) (st - 12));
        }
        if (bar == 0) addNote (dp, b0, 1.0, 49, 0.75f);
    }
    for (auto& part : song.parts)
        std::sort (part.begin(), part.end(), [] (const Note& x, const Note& y) { return x.start < y.start; });
    return song;
}

//==============================================================================
juce::MidiFile toMidiFile (const Song& song, const std::array<bool, NumParts>& which)
{
    constexpr int ppq = 960;
    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);

    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / song.bpm)), 0);
    tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    tempo.addEvent (juce::MidiMessage::keySignatureMetaEvent (0, song.scale == 1), 0);
    file.addTrack (tempo);

    for (int part = 0; part < NumParts; ++part)
    {
        if (! which[(size_t) part] || song.parts[(size_t) part].empty()) continue;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Hum-Cat " + partNames()[part]), 0);
        const int ch = partChannel (part);
        for (const auto& nn : song.parts[(size_t) part])
        {
            const auto on = (int) std::round (nn.start * ppq), off = (int) std::round ((nn.start + nn.length) * ppq) - 1;
            seq.addEvent (juce::MidiMessage::noteOn (ch, juce::jlimit (0, 127, nn.pitch), nn.velocity), on);
            seq.addEvent (juce::MidiMessage::noteOff (ch, juce::jlimit (0, 127, nn.pitch)), std::max (on + 1, off));
        }
        seq.updateMatchedPairs();
        file.addTrack (seq);
    }
    return file;
}
} // namespace humcat
