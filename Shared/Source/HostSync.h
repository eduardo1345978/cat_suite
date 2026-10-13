#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace cat
{
/** Snapshot of the host transport for one processBlock call. */
struct TransportState
{
    bool hostPlaying = false;     // host transport is rolling and reports a ppq position
    bool running = false;         // hostPlaying OR the internal free-running clock is active
    bool jumped = false;          // position discontinuity (loop / locate / start)
    double bpm = 120.0;
    double ppqStart = 0.0;        // ppq at sample 0 of the block
    double ppqPerSample = 0.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    double barLengthPpq = 4.0;
    double lastBarStartPpq = 0.0;
};

/**
    Reads Cubase's AudioPlayHead once per block and provides sample-accurate ppq.
    When the host is stopped, a free-running clock can be started (e.g. on the first
    held key) so patterns, LFOs and envelopes keep working in the Standalone app and
    while auditioning with the transport stopped.
*/
class HostSync
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        freeRunning = false;
        freePpq = 0.0;
        expectedNextPpq = -1.0;
        wasRunning = false;
    }

    void beginBlock (juce::AudioPlayHead* playHead, int numSamples)
    {
        blockSize = numSamples;
        TransportState t;
        t.bpm = lastBpm;
        bool hasPpq = false;

        if (playHead != nullptr)
        {
            if (auto pos = playHead->getPosition())
            {
                if (auto bpm = pos->getBpm()) if (*bpm > 1.0) t.bpm = *bpm;
                if (auto sig = pos->getTimeSignature())
                {
                    t.timeSigNumerator = juce::jmax (1, sig->numerator);
                    t.timeSigDenominator = juce::jmax (1, sig->denominator);
                }
                if (auto bar = pos->getPpqPositionOfLastBarStart()) t.lastBarStartPpq = *bar;

                if (pos->getIsPlaying())
                {
                    if (auto ppq = pos->getPpqPosition())
                    {
                        t.ppqStart = *ppq;
                        hasPpq = true;
                    }
                }
            }
        }

        lastBpm = t.bpm;
        t.ppqPerSample = t.bpm / (60.0 * sampleRate);
        t.barLengthPpq = 4.0 * (double) t.timeSigNumerator / (double) t.timeSigDenominator;
        t.hostPlaying = hasPpq;

        if (t.hostPlaying)
        {
            freeRunning = false;
            t.running = true;
        }
        else if (freeRunning)
        {
            t.ppqStart = freePpq;
            t.running = true;
            t.lastBarStartPpq = std::floor (freePpq / t.barLengthPpq) * t.barLengthPpq;
        }

        const double tolerance = juce::jmax (0.01, 4.0 * t.ppqPerSample);
        t.jumped = t.running && (! wasRunning || std::abs (t.ppqStart - expectedNextPpq) > tolerance);

        state = t;
    }

    /** Starts the internal clock so that ppq == 0 at the given sample of this block. */
    void startFreeRunAt (int sampleOffset)
    {
        if (state.hostPlaying || freeRunning)
            return;

        freeRunning = true;
        state.running = true;
        state.jumped = true;
        state.ppqStart = -(double) sampleOffset * state.ppqPerSample;
        state.lastBarStartPpq = 0.0;
    }

    void stopFreeRun()
    {
        if (! state.hostPlaying)
        {
            freeRunning = false;
            state.running = false;
        }
    }

    void endBlock()
    {
        const double end = state.ppqStart + blockSize * state.ppqPerSample;
        if (freeRunning && ! state.hostPlaying)
            freePpq = end;

        expectedNextPpq = end;
        wasRunning = state.running;
    }

    const TransportState& get() const noexcept      { return state; }
    bool isRunning() const noexcept                 { return state.running; }
    bool isFreeRunning() const noexcept             { return freeRunning && ! state.hostPlaying; }
    double ppqAt (int sample) const noexcept        { return state.ppqStart + sample * state.ppqPerSample; }
    double samplesPerPpq() const noexcept           { return state.ppqPerSample > 0.0 ? 1.0 / state.ppqPerSample : sampleRate / 2.0; }
    double getSampleRate() const noexcept           { return sampleRate; }

    /** Sample offset (may be outside the block) where the given ppq occurs. */
    double sampleForPpq (double ppq) const noexcept { return (ppq - state.ppqStart) * samplesPerPpq(); }

private:
    TransportState state;
    double sampleRate = 44100.0;
    double lastBpm = 120.0;
    double freePpq = 0.0;
    double expectedNextPpq = -1.0;
    int blockSize = 0;
    bool freeRunning = false;
    bool wasRunning = false;
};

} // namespace cat
