// Renders every preset of one sound Cat offline and measures the output:
// peak, RMS, low-bass share (< 60 Hz), sub-bass (< 25 Hz), DC, and what is left after the notes stop.
// One line per preset; "!!" marks anything that could hurt ears or speakers.
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
using namespace juce;

struct PH : AudioPlayHead
{
    double ppq = 0.0;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i;
        i.setBpm (120.0); i.setIsPlaying (true); i.setPpqPosition (ppq);
        i.setTimeSignature (TimeSignature { 4, 4 });
        i.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        return i;
    }
};

struct Meter
{
    double sum = 0, low = 0, sub = 0, dc = 0; float peak = 0; int64 n = 0;
    float lp60 = 0, lp25 = 0, a60, a25;
    explicit Meter (double sr) : a60 ((float) (1.0 - std::exp (-MathConstants<double>::twoPi * 60.0 / sr))),
                                 a25 ((float) (1.0 - std::exp (-MathConstants<double>::twoPi * 25.0 / sr))) {}
    void add (float x)
    {
        if (! std::isfinite (x)) x = 99.0f;
        peak = jmax (peak, std::abs (x));
        sum += (double) x * x; dc += x; ++n;
        lp60 += a60 * (x - lp60); low += (double) lp60 * lp60;
        lp25 += a25 * (x - lp25); sub += (double) lp25 * lp25;
    }
    double rms() const { return n ? std::sqrt (sum / n) : 0.0; }
    double lowShare() const { return sum > 1e-12 ? low / sum : 0.0; }
    double subShare() const { return sum > 1e-12 ? sub / sum : 0.0; }
    double dcv() const { return n ? dc / n : 0.0; }
};

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    std::unique_ptr<AudioProcessor> p (createPluginFilter());
    // SoundCheck [sampleRate] [blockSize] [preset]
    const double sr = argc > 1 ? String (argv[1]).getDoubleValue() : 48000.0;
    const int bs = argc > 2 ? String (argv[2]).getIntValue() : 512;
    p->setPlayConfigDetails (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels(), sr, bs);
    PH ph; p->setPlayHead (&ph);
    const int only = argc > 3 ? String (argv[3]).getIntValue() : -1;
    std::cout << p->getName() << " at " << sr << " Hz, block " << bs << std::endl;
    std::cout << "preset;name;peak;rms;low<60Hz%;sub<25Hz%;dc;tailRms;idleRms;flags" << std::endl;
    int bad = 0;
    for (int prog = 0; prog < jmax (1, p->getNumPrograms()); ++prog)
    {
        if (only >= 0 && prog != only) continue;
        p->releaseResources();
        if (p->getNumPrograms() > 0) p->setCurrentProgram (prog);
        if (argc > 4 && String (argv[4]) == "torture")
            for (auto* prm : p->getParameters())
            {
                const auto n = prm->getName (100);
                if (n.containsIgnoreCase ("Feedback") || n.containsIgnoreCase ("Drive") || n.containsIgnoreCase ("Mix") || n == "Room")
                    prm->setValueNotifyingHost (n == "Room" ? (float) ((prog % 7) / 6.0) : 1.0f);
            }
        p->prepareToPlay (sr, bs);
        AudioBuffer<float> buf (jmax (2, jmax (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels())), bs);
        Meter idle (sr), play (sr), tail (sr);
        String flags;
        auto run = [&] (Meter& m, int blocks, std::function<void (MidiBuffer&, int)> events)
        {
            for (int b = 0; b < blocks; ++b)
            {
                buf.clear(); MidiBuffer midi; events (midi, b);
                p->processBlock (buf, midi);
                for (int i = 0; i < bs; ++i) m.add (0.5f * (buf.getSample (0, i) + buf.getSample (1, i)));
                ph.ppq += bs / (sr * 0.5);
            }
        };
        const int sec = (int) (sr / bs);
        run (idle, sec, [] (MidiBuffer&, int) {});
        run (play, 6 * sec, [] (MidiBuffer& m, int b)
        {
            if (b == 0) for (int n : { 48, 60, 64, 67 }) m.addEvent (MidiMessage::noteOn (1, n, (uint8) 100), 0);
            if (b == 40) for (int n : { 72, 76 }) m.addEvent (MidiMessage::noteOn (1, n, (uint8) 90), 10);
            if (b == 120) for (int n : { 72, 76 }) m.addEvent (MidiMessage::noteOff (1, n), 0);
            if (b == 200) m.addEvent (MidiMessage::noteOn (1, 36, (uint8) 127), 3);   // a hard low note
            if (b == 300) m.addEvent (MidiMessage::noteOff (1, 36), 0);
        });
        // Cubase-like expressive playing: reset CCs, sustain pedal, mod wheel, pitch bend,
        // aftertouch, low notes, other channels (MPE)
        Meter expr (sr);
        run (expr, 6 * sec, [] (MidiBuffer& m, int b)
        {
            if (b == 0)
            {
                m.addEvent (MidiMessage::controllerEvent (1, 121, 0), 0);
                m.addEvent (MidiMessage::controllerEvent (1, 64, 127), 1);
                for (int n : { 24, 36, 43, 52 }) m.addEvent (MidiMessage::noteOn (1, n, (uint8) 120), 2);
                for (int ch = 2; ch <= 4; ++ch) m.addEvent (MidiMessage::noteOn (ch, 55 + ch * 3, (uint8) 100), 3);
            }
            m.addEvent (MidiMessage::pitchWheel (1, 8192 + (int) (8000.0 * std::sin (b * 0.07))), 5);
            m.addEvent (MidiMessage::pitchWheel (2 + b % 3, 8192 + (int) (4000.0 * std::sin (b * 0.11))), 6);
            m.addEvent (MidiMessage::controllerEvent (1, 1, (b * 3) % 128), 7);
            m.addEvent (MidiMessage::channelPressureChange (1, (b * 5) % 128), 8);
            if (b % 7 == 0) m.addEvent (MidiMessage::controllerEvent (1, 11, 64 + (b % 64)), 9);
            if (b % 20 == 0) m.addEvent (MidiMessage::noteOn (1, 30 + (b / 20) % 40, (uint8) 127), 10);
            if (b % 20 == 10) m.addEvent (MidiMessage::noteOff (1, 30 + (b / 20) % 40), 10);
            if (b == 300) { m.addEvent (MidiMessage::controllerEvent (1, 64, 0), 0); for (int ch = 1; ch <= 4; ++ch) m.addEvent (MidiMessage::allNotesOff (ch), 1); }
        });
        if (expr.peak > 0.99f) flags << " expr:CLIP";
        if (expr.lowShare() > 0.5 && expr.rms() > 0.02) flags << " expr:BASSY";
        if (expr.subShare() > 0.25 && expr.rms() > 0.02) flags << " expr:SUB";
        if (std::abs (expr.dcv()) > 0.02) flags << " expr:DC";

        run (tail, 4 * sec, [] (MidiBuffer& m, int b) { if (b == 0) for (int n : { 48, 60, 64, 67 }) m.addEvent (MidiMessage::noteOff (1, n), 0); });
        Meter all (sr);
        if (play.peak > 0.99f) flags << " CLIP";
        if (play.lowShare() > 0.5 && play.rms() > 0.02) flags << " BASSY";
        if (play.subShare() > 0.25 && play.rms() > 0.02) flags << " SUB";
        if (std::abs (play.dcv()) > 0.02) flags << " DC";
        if (play.peak > 50.0f) flags << " NAN";
        if (tail.rms() > 0.05 && tail.lowShare() > 0.5) flags << " BASS-AFTER-RELEASE";
        if (flags.isNotEmpty()) { flags = "!!" + flags; ++bad; }
        std::cout << prog << ";" << p->getProgramName (prog) << ";" << String (play.peak, 3) << ";" << String (play.rms(), 4) << ";"
                  << String (100.0 * play.lowShare(), 1) << ";" << String (100.0 * play.subShare(), 1) << ";" << String (play.dcv(), 4) << ";"
                  << String (tail.rms(), 4) << "(" << String (100.0 * tail.lowShare(), 0) << "% low);expr " << String (expr.peak, 2) << "/" << String (expr.rms(), 3) << "/" << String (100.0 * expr.lowShare(), 0) << "% low;" << String (idle.rms(), 4) << ";" << flags << std::endl;
    }
    std::cout << "flagged presets: " << bad << std::endl;
    return 0;
}
