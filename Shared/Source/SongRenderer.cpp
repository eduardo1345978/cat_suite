#include "SongRenderer.h"

namespace cat::muse
{
SoundSet defaultSounds (int genre)
{
    switch (genre)
    {
        case Pop:       return { 1, 0, 3, 2, 2 };
        case LoFi:      return { 1, 0, 2, 1, 1 };
        case Trap:      return { 0, 0, 1, 2, 2 };
        case House:     return { 2, 3, 0, 2, 2 };
        case Ambient:   return { 0, 0, 2, 3, 2 };
        case Jazz:      return { 1, 3, 3, 1, 1 };
        case Rock:      return { 3, 1, 3, 3, 1 };
        case Aero:       return { 0, 0, 2, 5, 5 };   // glassy pad, glass bells
        case Synthwave:  return { 0, 1, 3, 2, 2 };   // pad, saw bass, plucky arps
        case BossaNova:  return { 1, 3, 2, 1, 2 };   // e-piano, plucked bass, soft kit
        case Chiptune:   return { 4, 1, 1, 4, 4 };   // square waves
        case Orchestral: return { 3, 0, 3, 3, 3 };   // strings
        case CityPop:    return { 1, 3, 3, 2, 1 };   // e-piano, plucked bass, live kit
        case Reggae:     return { 1, 0, 3, 2, 0 };   // e-piano skank, round sub bass
        case Folk:       return { 2, 3, 3, 2, 3 };   // plucked "guitar", strings answering
        case DreamPop:   return { 0, 0, 2, 5, 0 };   // hazy pad, glass bell melody
        case Funk:       return { 1, 1, 3, 2, 1 };   // e-piano stabs, saw bass
        default:         return { 3, 0, 3, 3, 3 };   // Cinematic
    }
}

void SongRenderer::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    for (auto* k : { &chords, &melody, &counter }) k->prepare (sampleRate);
    bass.prepare (sampleRate);
    drums.prepare (sampleRate);
    juce::Reverb::Parameters p;
    p.roomSize = 0.7f; p.damping = 0.45f; p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.width = 1.0f;
    reverb.setParameters (p);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    limiter.prepare (sampleRate);
    work.setSize (2, juce::jmax (1, maxBlock));
    send.setSize (2, juce::jmax (1, maxBlock));
    for (auto& b : pending) b.ensureSize (2048);
}

void SongRenderer::setSounds (const SoundSet& s)
{
    sounds = s;
    chords.setSound (s.chords);
    melody.setSound (s.melody);
    counter.setSound (s.counter);
    bass.setSound (s.bass);
    drums.setSound (s.drums);
}

void SongRenderer::allOff()
{
    for (auto* k : { &chords, &melody, &counter }) k->allOff();
    bass.allOff();
    drums.allOff();
}

void SongRenderer::handle (int part, const juce::MidiBuffer& m)
{
    if (! juce::isPositiveAndBelow (part, (int) NumParts)) return;
    pending[(size_t) part].addEvents (m, 0, -1, 0);
}

void SongRenderer::render (float* l, float* r, int n)
{
    if (n > work.getNumSamples()) { work.setSize (2, n, false, false, true); send.setSize (2, n, false, false, true); }
    juce::FloatVectorOperations::clear (l, n);
    juce::FloatVectorOperations::clear (r, n);
    send.clear (0, n);
    for (int p = 0; p < NumParts; ++p)
    {
        work.clear (0, n);
        float* wl = work.getWritePointer (0);
        float* wr = work.getWritePointer (1);
        // play the MIDI in time inside the block
        int pos = 0;
        auto renderTo = [&] (int upTo)
        {
            if (upTo <= pos) return;
            const int len = upTo - pos;
            switch (p)
            {
                case Chords:  chords.render (wl + pos, wr + pos, len); break;
                case Bass:    bass.render (wl + pos, wr + pos, len); break;
                case Drums:   drums.render (wl + pos, wr + pos, len); break;
                case Melody:  melody.render (wl + pos, wr + pos, len); break;
                default:      counter.render (wl + pos, wr + pos, len); break;
            }
            pos = upTo;
        };
        for (const auto meta : pending[(size_t) p])
        {
            renderTo (juce::jlimit (0, n, meta.samplePosition));
            const auto msg = meta.getMessage();
            switch (p)
            {
                case Chords:  chords.handle (msg); break;
                case Bass:    bass.handle (msg); break;
                case Drums:   drums.handle (msg); break;
                case Melody:  melody.handle (msg); break;
                default:      counter.handle (msg); break;
            }
        }
        renderTo (n);
        pending[(size_t) p].clear();

        // the keys sounds are not equally loud: even them out
        static const float keysTrim[4] = { 1.0f, 0.6f, 1.0f, 0.75f };
        float trim = 1.0f;
        if (p == Chords) trim = keysTrim[juce::jlimit (0, 3, sounds.chords)];
        if (p == Melody) trim = keysTrim[juce::jlimit (0, 3, sounds.melody)];
        if (p == Counter) trim = keysTrim[juce::jlimit (0, 3, sounds.counter)];
        const float g = mute[(size_t) p] ? 0.0f : levels[(size_t) p] * trim;
        float peak = 0.0f;
        const float sendAmt = p == Bass || p == Drums ? reverbSend * 0.25f : reverbSend;
        for (int i = 0; i < n; ++i)
        {
            const float a = wl[i] * g, b = wr[i] * g;
            l[i] += a; r[i] += b;
            send.getWritePointer (0)[i] += a * sendAmt;
            send.getWritePointer (1)[i] += b * sendAmt;
            peak = std::max (peak, std::abs (a));
        }
        meters[(size_t) p] = std::max (peak, meters[(size_t) p] * 0.9f);
    }
    reverb.processStereo (send.getWritePointer (0), send.getWritePointer (1), n);
    for (int i = 0; i < n; ++i)
    {
        l[i] += send.getReadPointer (0)[i];
        r[i] += send.getReadPointer (1)[i];
    }
    limiter.process (l, r, n);
}

bool SongRenderer::renderToWav (const Song& song, const SoundSet& sounds, const juce::File& file, double sampleRate,
                                int onlyPart, std::function<bool (float)> progress)
{
    constexpr int block = 512;
    SongRenderer r;
    r.prepare (sampleRate, block);
    r.setSounds (sounds);
    if (onlyPart >= 0) for (int p = 0; p < NumParts; ++p) r.mute[(size_t) p] = p != onlyPart;

    // a fixed copy of the song (endless: just this piece)
    auto s = std::make_shared<Song> (song);
    s->settings.endless = false;
    SongPlayer player;
    player.setSong (s);

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> os (file.createOutputStream());
    if (os == nullptr) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (os.get(), sampleRate, 2, 24, {}, 0));
    if (writer == nullptr) return false;
    os.release();

    const double ppqPerSample = song.bpm / 60.0 / sampleRate;
    const int64_t songSamples = (int64_t) std::floor (song.lengthBeats / ppqPerSample);
    const int64_t tail = (int64_t) (sampleRate * 3.0);
    juce::AudioBuffer<float> buf (2, block);
    std::array<juce::MidiBuffer, NumParts> midi;
    std::array<juce::MidiBuffer*, NumParts> ptrs;
    for (int p = 0; p < NumParts; ++p) ptrs[(size_t) p] = &midi[(size_t) p];

    for (int64_t pos = 0; pos < songSamples + tail; pos += block)
    {
        const int n = (int) std::min<int64_t> (block, songSamples + tail - pos);
        for (auto& m : midi) m.clear();
        if (pos < songSamples)
        {
            const int inSong = (int) std::min<int64_t> (n, songSamples - pos);
            player.process ((double) pos * ppqPerSample, ppqPerSample, inSong, true, ptrs);
        }
        else if (pos - block < songSamples)
            player.process (0.0, ppqPerSample, n, false, ptrs);   // stop: every note off
        for (int p = 0; p < NumParts; ++p) r.handle (p, midi[(size_t) p]);
        r.render (buf.getWritePointer (0), buf.getWritePointer (1), n);
        if (! writer->writeFromAudioSampleBuffer (buf, 0, n)) return false;
        if (progress && ! progress ((float) ((double) pos / (double) (songSamples + tail)))) return false;
    }
    return true;
}
} // namespace cat::muse
