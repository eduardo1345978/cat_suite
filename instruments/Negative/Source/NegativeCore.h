#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <atomic>
#include <vector>

/**
    Negative's maths: a picture becomes sound so that the sound's spectrogram shows the picture (time left to
    right, pitch bottom to top), and a sound becomes a picture (its spectrogram).

    Three ways to make the sound:
      EXACT    - every pixel is the loudness of one frequency at one moment (noise shaped by the picture, played back
                 through an inverse FFT): the picture comes out sharp in any spectrogram, it sounds like air and hiss.
      MUSICAL  - the picture is played on the notes of a key: bright spots are notes, bright areas chords.
      TEXTURE  - the picture is a soft pad: many slowly moving, slightly detuned partials.
*/
namespace negative
{
juce::StringArray modeNames();
juce::StringArray lengthNames();
double lengthBars (int index);
juce::StringArray keyNames();
juce::StringArray scaleNames();

/** A picture as brightness values 0..1, row 0 = the top (the highest frequency). */
struct Gray
{
    int w = 0, h = 0;
    std::vector<float> v;
    float at (float x, float y) const noexcept;   // bilinear, clamped
    bool empty() const noexcept { return w <= 0 || h <= 0; }
};

/** The picture made ready: brightness, CONTRAST (0.5 = as it is), SOFT (blur), NEGATIVE (inverted). */
Gray prepare (const juce::Image& photo, float contrast, float soft, bool invert, int maxW = 1200, int maxH = 400);

struct RenderSettings
{
    int mode = 0;
    float lowHz = 40.0f, highHz = 16000.0f;
    double seconds = 8.0, sampleRate = 48000.0;
    int key = 0, scale = 1;
};

/** The picture as sound (stereo, peak -1 dBFS; silence for a black picture). cancel: stop early (returns empty). */
juce::AudioBuffer<float> render (const Gray&, const RenderSettings&, const std::atomic<bool>* cancel = nullptr);

/** A spectrogram of the sound as a picture: log frequency lowHz..highHz bottom to top, -90..0 dB = black..white. */
juce::Image analyse (const juce::AudioBuffer<float>&, double sampleRate, float lowHz, float highHz, int width, int height);

/** Grayscale image <-> base64 PNG (for the saved project). */
juce::String toBase64Png (const juce::Image&);
juce::Image fromBase64Png (const juce::String&);
} // namespace negative
