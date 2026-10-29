#include "Templates.h"
#include "Music.h"

namespace catbox
{
namespace
{
// lanes: 0 kick, 1 snare, 2 clap, 3 closed hat, 4 open hat, 5-7 toms, 8 rim, 9 cowbell, 10 crash, 11 ride,
//        12 shaker, 13 conga, 14 snap, 15 zap
// steps: x = accent, o = hit, g = ghost (soft), . = nothing
struct DrumTemplate { const char* name; int tempo; std::vector<std::pair<int, const char*>> lanes; };

const std::vector<DrumTemplate>& drums()
{
    static const std::vector<DrumTemplate> t = {
        { "House", 124, { { 0, "x...x...x...x..." }, { 2, "....o.......o..." }, { 3, ".g.g.g.g.g.g.g.g" }, { 4, "..o...o...o...o." } } },
        { "Techno", 132, { { 0, "x...x...x...x..." }, { 2, "....g.......g..." }, { 4, "..o...o...o...o." }, { 11, "o.o.o.o.o.o.o.o." }, { 8, "...g......g....." } } },
        { "Trap", 140, { { 0, "x......x..x....." }, { 2, "........x......." }, { 3, "o.o.o.o.o.oooo.o" }, { 14, "............o..." } } },
        { "Hip hop", 90, { { 0, "x......o.o......" }, { 1, "....x.......x..." }, { 3, "o.o.o.o.o.o.o.o." }, { 4, "..............o." } } },
        { "Breakbeat", 128, { { 0, "x.x.......xo...." }, { 1, "....x..g.g..x..g" }, { 3, "o.o.o.o.o.o.o.o." }, { 10, "x..............." } } },
        { "Drum & bass", 172, { { 0, "x.........x....." }, { 1, "....x.......x..." }, { 3, "o.o.o.o.o.o.o.o." }, { 11, "g...g...g...g..." } } },
        { "Reggaeton", 96, { { 0, "x...x...x...x..." }, { 8, "...o..o....o..o." }, { 3, "o.o.o.o.o.o.o.o." } } },
        { "Disco", 118, { { 0, "x...x...x...x..." }, { 1, "....o.......o..." }, { 3, "g.g.g.g.g.g.g.g." }, { 4, "..o...o...o...o." }, { 13, "...g...g...g..g." } } },
        { "Electro", 120, { { 0, "x.....o...x....." }, { 1, "....x.......x..." }, { 3, "o.o.o.o.o.o.o.o." }, { 9, "..o.......o....." } } },
        { "Rock", 110, { { 0, "x.......x.x....." }, { 1, "....x.......x..." }, { 3, "o.o.o.o.o.o.o.o." }, { 10, "x..............." } } },
        { "Afro", 108, { { 0, "x..o..x...o....." }, { 13, "..o.o..oo.o..o.o" }, { 12, "gogogogogogogogo" }, { 8, "o..o..o...o.o..." }, { 9, "x...o...x...o.o." } } },
        { "Minimal", 122, { { 0, "x...x...x...x..." }, { 8, "...o......o....." }, { 12, "g.ggg.ggg.ggg.gg" }, { 14, ".......o........" } } },
        { "Half time", 75, { { 0, "x.........o....." }, { 1, "........x......." }, { 3, "o...o...o...o..." } } },
        { "Shuffle", 100, { { 0, "x.....o...x....." }, { 1, "....x.......x..." }, { 3, "o..oo..oo..oo..o" } } },
        { "Only hats", 120, { { 3, "o.o.o.o.o.o.o.o." }, { 4, "..o...o...o...o." } } },
        // braindance: broken breaks (add ratchets / rolls on top: SEQ, knob 11 on a step)
        { "Drill n bass", 165, { { 0, "x.o...x..o.x...." }, { 1, "....x.gg.gx.g.xg" }, { 3, "oooooooooooooooo" }, { 15, "...........o..o." } } },
        { "Braindance", 138, { { 0, "x..x..o...x..o.." }, { 1, "....x..g..g.x..." }, { 8, "o.o..oo.o.o..o.o" }, { 14, ".x.....x....x..." }, { 4, "......o........o" } } },
        { "Ambient tick", 96, { { 0, "x.......o......." }, { 8, "..o.g...o..g..o." }, { 12, "g.gg.g.gg.g.g.gg" } } },
    };
    return t;
}

// a line: scale degrees from the root (or . = nothing), ' = up an octave, , = down, ~ = slide into the next, ! = accent
struct BassTemplate { const char* name; const char* steps[16]; };

const std::vector<BassTemplate>& basses()
{
    static const std::vector<BassTemplate> t = {
        { "Root pulse", { "0", ".", "0", ".", "0", ".", "0", ".", "0", ".", "0", ".", "0", ".", "0", "." } },
        { "Offbeat", { ".", ".", "0", ".", ".", ".", "0", ".", ".", ".", "0", ".", ".", ".", "0!", "." } },
        { "Octaves", { "0", "0'", "0", "0'", "0", "0'", "0", "0'", "3", "3'", "3", "3'", "4", "4'", "4", "4'" } },
        { "Acid 1", { "0!", ".", "0", "0'~", "2", "0", ".", "0", "4!", "0", "0'", ".", "2~", "3", "0", "." } },
        { "Acid 2", { "0", "0'", "6,", "0~", "0'!", ".", "2", "0", "0", "4~", "3", "0'!", ".", "0", "6,~", "0" } },
        { "Walking", { "0", ".", "2", ".", "4", ".", "5", ".", "4", ".", "2", ".", "1", ".", "6,", "." } },
        { "Synthwave", { "0", "0", "0", "0", "0", "0", "0", "0", "5,", "5,", "5,", "5,", "6,", "6,", "6,", "6," } },
        { "Reggae", { ".", ".", "0", ".", "0", ".", ".", "2", ".", ".", "4", ".", "4", ".", "2", "." } },
        { "Funk", { "0!", ".", ".", "0", ".", "0'", "6,", "0", ".", ".", "2", "3", "4~", "0'", ".", "0" } },
        { "House bass", { ".", ".", "0", "0", ".", ".", "0", ".", ".", ".", "0", "0", ".", "4", "3", "." } },
        { "Long notes", { "0", ".", ".", ".", ".", ".", ".", ".", "5,", ".", ".", ".", "3", ".", "4", "." } },
        { "Arp up", { "0", "2", "4", "0'", "2'", "4'", "0'", "4", "0", "2", "4", "0'", "2'", "4'", "0'", "4" } },
        { "Analog acid", { "0!", "0'~", "0", "3", ".", "0'!", "6,~", "0", "0", "4'~", "4", ".", "0'!", "2~", "1", "0" } },
        { "Braindance", { "0", "0''", ".", "6", "0'~", "0", "4,", ".", "0'!", ".", "2'", "0", "6,~", "0'", "3'", "." } },
    };
    return t;
}
} // namespace

juce::StringArray drumTemplateNames()
{
    juce::StringArray n;
    for (const auto& d : drums()) n.add (d.name);
    return n;
}

void applyDrumTemplate (Pattern& p, int index)
{
    const auto& all = drums();
    const auto& t = all[(size_t) juce::jlimit (0, (int) all.size() - 1, index)];
    for (auto& lane : p.drums) lane.fill (0);
    for (auto& lane : p.rolls) lane.fill (0);
    p.length = 16;
    for (const auto& [lane, s] : t.lanes)
        for (int i = 0; i < 16 && s[i] != 0; ++i)
            p.drums[(size_t) lane][(size_t) i] = s[i] == 'x' ? 122 : s[i] == 'o' ? 100 : s[i] == 'g' ? 55 : 0;
}

juce::StringArray bassTemplateNames()
{
    juce::StringArray n;
    for (const auto& b : basses()) n.add (b.name);
    return n;
}

void applyBassTemplate (Pattern& p, int index, int key, int scale)
{
    const auto& all = basses();
    const auto& t = all[(size_t) juce::jlimit (0, (int) all.size() - 1, index)];
    for (auto& s : p.steps) s.clear();
    p.length = 16;
    for (int i = 0; i < 16; ++i)
    {
        const juce::String tok (t.steps[i]);
        if (tok.startsWith (".")) continue;
        const int degree = tok.getIntValue();
        int oct = 3;   // C2
        if (tok.contains ("'")) ++oct;
        if (tok.contains (",")) --oct;
        auto& st = p.steps[(size_t) i];
        st.notes[0] = (int8_t) juce::jlimit (0, 127, music::noteOfDegree (key, scale, oct, degree));
        st.vel = tok.contains ("!") ? 120 : 100;
        st.accent = tok.contains ("!");
        st.slide = tok.contains ("~");
        st.len = 70;
    }
}
} // namespace catbox
