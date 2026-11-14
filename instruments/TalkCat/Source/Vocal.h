#pragma once

#include <array>
#include <cmath>
#include <cstdint>

/**
    Talk-Cat voice model (from Sing-Cat): a physically modelled singer.

      Glottis  ->  vocal tract (Kelly-Lochbaum waveguide, 1 section per sample step)
                     |-- nasal branch, coupled at the velum by a 3-way junction
                     '-> lips + nostrils -> output

    The vocal-tract shape is described by 8 log-area control points from larynx to lips.
    Eight anchor vowels were fitted offline so that a 17 cm tract gives adult formants
    (e.g. /i/ 270/2190 Hz, /a/ 730/1130 Hz, /u/ 305/890 Hz); any point of the 2-D vowel
    chart blends the anchors. Shorter tracts (women, children, cats) raise all formants.
*/
namespace talkcat
{
constexpr int numCtrl = 8;
constexpr int maxSections = 72;
constexpr int maxNasal = 40;
constexpr float speedOfSound = 34300.0f;   // cm/s

//==============================================================================
struct VowelAnchor
{
    const char* label;
    float x, y;                          // chart position: x 0 = front .. 1 = back, y 0 = close .. 1 = open
    std::array<float, numCtrl> area;     // cm^2, larynx .. lips
    float mouthOpen, mouthWidth;         // for the singing faces
};

const std::array<VowelAnchor, 8>& vowelAnchors();

/** Tract shape for a point of the vowel chart. */
struct VowelShape
{
    std::array<float, numCtrl> logArea {};
    float mouthOpen = 0.5f, mouthWidth = 0.6f;
};

VowelShape shapeAt (float x, float y);

//==============================================================================
/** Small fast noise source (xorshift), deterministic per seed. */
struct Noise
{
    uint32_t s = 0x12345678u;
    void seed (uint32_t v) noexcept { s = v ? v : 0x9e3779b9u; }
    float next() noexcept   // -1..1
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
    float uni() noexcept { return 0.5f + 0.5f * next(); }
};

//==============================================================================
/**
    Glottal source: Rosenberg-style flow pulse (output is its derivative, which already
    includes the lip radiation tilt), with tension, jitter, shimmer, rasp (period doubling)
    and flow-modulated aspiration noise.
*/
class Glottis
{
public:
    void reset (uint32_t seed) noexcept;

    /** f0 in Hz, sampleRate = tract step rate. Returns glottal flow derivative + noise. */
    float process (float f0, float sampleRate) noexcept;

    float tenseness = 0.6f;    // 0 = lax / breathy .. 1 = pressed
    float voicing = 1.0f;      // amplitude of the voiced part
    float aspiration = 0.05f;  // noise amount (relative)
    float jitter = 0.005f, shimmer = 0.03f, rasp = 0.0f;

private:
    void newPeriod() noexcept;

    Noise noise;
    float phase = 0.0f;
    float periodScale = 1.0f, ampScale = 1.0f;
    bool oddPeriod = false;
    float openQ = 0.6f;
    float hp = 0.0f, lp = 0.0f, lastNoise = 0.0f;
};

//==============================================================================
/** Kelly-Lochbaum vocal tract with a nasal side branch. */
class Tract
{
public:
    /** Number of sections defines the tract length (length = n * c / stepRate). */
    void prepare (int numSections) noexcept;

    /** Changes the tract length while it sounds: areas and travelling waves are resampled (no reset). */
    void resize (int numSections) noexcept;
    void clear() noexcept;
    int size() const noexcept { return n; }

    /** Target areas from 8 log-area control points (+ velum opening in cm^2). Smoothed internally. */
    void setTarget (const std::array<float, numCtrl>& logArea, float velumArea) noexcept;

    /** Moves the current areas towards the target (call at control rate). */
    void smooth (float amount) noexcept;

    /** One step: glottal input (+ optional turbulence noise at a section) in, radiated (lips + nose) sound out. */
    float step (float glottalIn, float noise = 0.0f, int noiseIndex = -1) noexcept;

    /** Section index for a position along the tract (0 = larynx .. 1 = lips). */
    int sectionAt (float position) const noexcept;

    float glottalReflection = 0.75f, lipReflection = -0.85f, loss = 0.9992f;

    /** Frequency-dependent lip radiation: low frequencies reflect back almost fully, highs radiate.
        lipCoef = one-pole split frequency (0 = off -> constant lipReflection). */
    float lipLow = -0.97f, lipHigh = -0.62f, lipCoef = 0.0f;

private:
    void updateReflections() noexcept;

    int n = 24, nn = 10, velumIndex = 10;
    std::array<float, maxSections> area {}, target {}, k {};
    std::array<float, maxSections> R {}, L {};
    std::array<float, maxSections + 1> jR {}, jL {};
    std::array<float, maxNasal> nArea {}, nk {}, nR {}, nL {};
    std::array<float, maxNasal + 1> njR {}, njL {};
    float velum = 0.0f, velumTarget = 0.0f;
    float lipLp = 0.0f;
};

} // namespace talkcat
