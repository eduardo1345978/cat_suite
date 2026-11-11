#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PresetBar.h"
#include "SafetyLimiter.h"
#include "CryptSynth.h"
#include "CryptSfx.h"

/**
    Crypt: dark chip music and sound effects for a dungeon game.
    SYNTH - old-console waves (NES pulse / triangle / noise, Game Boy wave tables), dark chords played at once or as
            fast arpeggios, the NES pitch sweep, 16-step volume, then the crypt: bits, sample rate, drive, gloom,
            a stone room and a dotted echo.
    SFX   - pick a kind (sword, spell, door, trap, death ...), GENERATE, MUTATE, PLAY, SAVE as a .wav for the game.
*/
class CryptProcessor : public juce::AudioProcessor
{
public:
    CryptProcessor();
    ~CryptProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Crypt"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int i) override { if (i != currentPreset.load()) applyPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;

    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> values; };
    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPreset { -1 };
    cat::PresetManager presetManager { *this, "Crypt",
        { [] { juce::StringArray n; for (auto& p : presets()) n.add (p.name); return n; },
          [this] (int i) { applyPreset (i); updateHostDisplay (ChangeDetails().withProgramChanged (true)); },
          [this] { return currentPreset.load(); } } };

    //==== the sound-effect maker (message thread) ====
    tomb::Sfx sfx;                                   // the one on the screen
    std::vector<tomb::Sfx> history;                  // the last ones made (newest first)
    float sfxDarkness = 0.5f, sfxPitch = 0.0f, sfxLength = 1.0f;
    juce::File sfxFolder;                             // where SAVE TO GAME puts the files
    std::map<int, int> sfxCounter;                    // kind -> the next number
    int tab = 0;                                      // 0 synth, 1 sfx

    /** Makes a new effect of this kind (and remembers the old one). */
    void newSfx (tomb::Kind);
    void mutateSfx();
    /** The current effect at 44.1 kHz (what is saved). */
    juce::AudioBuffer<float> renderSfx (double sampleRate = 44100.0) const;
    /** Plays a sound through the plug-in's output (the preview). */
    void playPreview (juce::AudioBuffer<float>);
    /** Saves the current effect: into the game folder as "<kind>_NN.wav" (or to `file`). Returns the file or {}. */
    juce::File saveSfx (const juce::File& file = {});
    /** A whole pack for the game: every kind, 3 of each, into a new folder in the game folder. Returns it. */
    juce::File makePack();
    std::atomic<float> previewPos { -1.0f };          // 0..1 while the preview plays (for the screen)
    std::atomic<float> level { 0.0f };

    static float rateHz (float v);                    // the RATE knob 0..1 -> Hz

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    tomb::Synth synth;
    tomb::Crusher crusher;
    tomb::Space space;
    cat::SafetyLimiter limiter;
    double lastBpm = 120.0;
    std::vector<float> monoSpare;

    juce::SpinLock previewLock;
    juce::AudioBuffer<float> preview;
    int previewAt = 0;
    juce::uint32 sfxSeed = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CryptProcessor)
};
