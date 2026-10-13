#include "Score.h"

namespace talkcat
{
const LaneInfo& laneInfo (int lane)
{
    static const LaneInfo info[] = {
        { "Pitch",      -1200.0f, 1200.0f, 0.0f, "cents" },
        { "Dynamics",       0.0f,    2.0f, 1.0f, "x" },
        { "Breath",         0.0f,    1.0f, 0.0f, "" },
        { "Brightness",    -1.0f,    1.0f, 0.0f, "" },
        { "Gender",        -1.0f,    1.0f, 0.0f, "" },
        { "Tension",       -1.0f,    1.0f, 0.0f, "" },
        { "Growl",          0.0f,    1.0f, 0.0f, "" },
        { "Vibrato",        0.0f,    2.0f, 1.0f, "x" },
        { "Cat",            0.0f,    1.0f, 0.0f, "" },
        { "Mouth",         -1.0f,    1.0f, 0.0f, "" },
    };
    return info[juce::jlimit (0, numLanes - 1, lane)];
}

namespace
{
    float evalLane (const std::vector<CurvePoint>& pts, int lane, double beat)
    {
        if (pts.empty()) return laneInfo (lane).def;
        if (beat <= pts.front().beat) return pts.front().value;
        if (beat >= pts.back().beat) return pts.back().value;
        auto it = std::upper_bound (pts.begin(), pts.end(), beat, [] (double b, const CurvePoint& p) { return b < p.beat; });
        const auto& b = *it;
        const auto& a = *(it - 1);
        const double f = (beat - a.beat) / juce::jmax (1.0e-9, b.beat - a.beat);
        return (float) (a.value + (b.value - a.value) * f);
    }
}

void Score::sort()
{
    std::stable_sort (notes.begin(), notes.end(), [] (const ScoreNote& a, const ScoreNote& b) { return a.start < b.start; });
}

void Score::makeMonophonic()
{
    sort();
    for (size_t i = 0; i + 1 < notes.size(); ++i)
        if (notes[i].end() > notes[i + 1].start)
            notes[i].length = juce::jmax (1.0 / 64.0, notes[i + 1].start - notes[i].start);
    // drop notes that start at the same time (keep the later one in the list = the higher one after a sort by pitch)
    notes.erase (std::unique (notes.begin(), notes.end(), [] (const ScoreNote& a, const ScoreNote& b) { return std::abs (a.start - b.start) < 1.0e-6; }), notes.end());
}

int Score::addNote (ScoreNote n)
{
    n.id = nextId++;
    notes.push_back (n);
    sort();
    return n.id;
}

ScoreNote* Score::find (int id)
{
    for (auto& n : notes) if (n.id == id) return &n;
    return nullptr;
}

int Score::indexOf (int id) const
{
    for (size_t i = 0; i < notes.size(); ++i) if (notes[i].id == id) return (int) i;
    return -1;
}

float Score::laneValue (int lane, double beat) const { return evalLane (lanes[(size_t) lane], lane, beat); }
float CompiledScore::laneValue (int lane, double beat) const { return evalLane (lanes[(size_t) lane], lane, beat); }

void Score::setLaneRange (int lane, double from, double to, const std::vector<CurvePoint>& points)
{
    auto& pts = lanes[(size_t) lane];
    if (from > to) std::swap (from, to);
    pts.erase (std::remove_if (pts.begin(), pts.end(), [=] (const CurvePoint& p) { return p.beat >= from && p.beat <= to; }), pts.end());
    for (const auto& p : points) pts.push_back (p);
    std::sort (pts.begin(), pts.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.beat < b.beat; });
}

void Score::clearLaneRange (int lane, double from, double to)
{
    if (from > to) std::swap (from, to);
    auto& pts = lanes[(size_t) lane];
    const float def = laneInfo (lane).def;
    pts.erase (std::remove_if (pts.begin(), pts.end(), [=] (const CurvePoint& p) { return p.beat >= from && p.beat <= to; }), pts.end());
    // keep the curve outside the range: anchor default values at the edges
    if (! pts.empty())
    {
        pts.push_back ({ from, def });
        pts.push_back ({ to, def });
        std::sort (pts.begin(), pts.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.beat < b.beat; });
    }
}

juce::ValueTree Score::toValueTree() const
{
    juce::ValueTree t ("SCORE");
    t.setProperty ("length", lengthBeats, nullptr);
    t.setProperty ("startBar", startBar, nullptr);
    t.setProperty ("loop", loop, nullptr);
    t.setProperty ("nextId", nextId, nullptr);
    for (const auto& n : notes)
    {
        juce::ValueTree c ("NOTE");
        c.setProperty ("id", n.id, nullptr);
        c.setProperty ("start", n.start, nullptr);
        c.setProperty ("len", n.length, nullptr);
        c.setProperty ("pitch", n.pitch, nullptr);
        c.setProperty ("lyric", n.lyric, nullptr);
        if (n.phonemes.isNotEmpty()) c.setProperty ("ph", n.phonemes, nullptr);
        c.setProperty ("vel", n.velocity, nullptr);
        if (n.language >= 0) c.setProperty ("lang", n.language, nullptr);
        if (n.portaMs >= 0) c.setProperty ("porta", n.portaMs, nullptr);
        if (n.vibDepth >= 0) c.setProperty ("vibD", n.vibDepth, nullptr);
        if (n.vibRate >= 0) c.setProperty ("vibR", n.vibRate, nullptr);
        if (n.vibDelay >= 0) c.setProperty ("vibT", n.vibDelay, nullptr);
        if (n.consScale != 1.0f) c.setProperty ("cons", n.consScale, nullptr);
        if (n.consStrength >= 0) c.setProperty ("consS", n.consStrength, nullptr);
        t.appendChild (c, nullptr);
    }
    for (int l = 0; l < numLanes; ++l)
    {
        if (lanes[(size_t) l].empty()) continue;
        juce::ValueTree c ("LANE");
        c.setProperty ("index", l, nullptr);
        juce::String data;
        for (const auto& p : lanes[(size_t) l]) data << juce::String (p.beat, 4) << ":" << juce::String (p.value, 4) << " ";
        c.setProperty ("pts", data.trim(), nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

Score Score::fromValueTree (const juce::ValueTree& t)
{
    Score s;
    if (! t.isValid()) return s;
    s.lengthBeats = (double) t.getProperty ("length", 32.0);
    s.startBar = (int) t.getProperty ("startBar", 1);
    s.loop = (bool) t.getProperty ("loop", false);
    s.nextId = (int) t.getProperty ("nextId", 1);
    for (const auto& c : t)
    {
        if (c.hasType ("NOTE"))
        {
            ScoreNote n;
            n.id = (int) c.getProperty ("id", 0);
            n.start = (double) c.getProperty ("start", 0.0);
            n.length = (double) c.getProperty ("len", 1.0);
            n.pitch = (int) c.getProperty ("pitch", 60);
            n.lyric = c.getProperty ("lyric", "la").toString();
            n.phonemes = c.getProperty ("ph", "").toString();
            n.velocity = (float) c.getProperty ("vel", 0.8f);
            n.language = (int) c.getProperty ("lang", -1);
            n.portaMs = (float) c.getProperty ("porta", -1.0f);
            n.vibDepth = (float) c.getProperty ("vibD", -1.0f);
            n.vibRate = (float) c.getProperty ("vibR", -1.0f);
            n.vibDelay = (float) c.getProperty ("vibT", -1.0f);
            n.consScale = (float) c.getProperty ("cons", 1.0f);
            n.consStrength = (float) c.getProperty ("consS", -1.0f);
            s.notes.push_back (n);
            s.nextId = juce::jmax (s.nextId, n.id + 1);
        }
        else if (c.hasType ("LANE"))
        {
            const int l = juce::jlimit (0, numLanes - 1, (int) c.getProperty ("index", 0));
            for (const auto& tok : juce::StringArray::fromTokens (c.getProperty ("pts", "").toString(), " ", ""))
                if (tok.contains (":"))
                    s.lanes[(size_t) l].push_back ({ tok.upToFirstOccurrenceOf (":", false, false).getDoubleValue(),
                                                     tok.fromFirstOccurrenceOf (":", false, false).getFloatValue() });
        }
    }
    s.sort();
    return s;
}

void Score::fillLyrics (const juce::String& text, int firstIndex, Language language, int maxNotes)
{
    sort();
    const auto syls = textToSyllables (text, language);
    int ni = juce::jmax (0, firstIndex);
    for (size_t k = 0; k < syls.size() && ni < (int) notes.size() && maxNotes > 0; ++k, ++ni, --maxNotes)
    {
        auto& n = notes[(size_t) ni];
        const auto& s = syls[k];
        if (s.hold) { n.lyric = "-"; n.phonemes = {}; continue; }
        if (s.breath) { n.lyric = "br"; n.phonemes = {}; continue; }
        n.lyric = juce::String::fromUTF8 (s.text);
        // a fragment of a longer word ("nkle") would not be read back the same way: keep its phonemes
        const auto again = textToSyllables (n.lyric, language);
        n.phonemes = (again.size() == 1 && phonemeString (again[0]) == phonemeString (s)) ? juce::String() : "[" + phonemeString (s) + "]";
        n.language = -1;
    }
}

juce::MidiMessageSequence Score::toMidi() const
{
    juce::MidiMessageSequence seq;
    constexpr double tpq = 960.0;
    for (const auto& n : notes)
    {
        const double on = n.start * tpq, off = n.end() * tpq;
        auto lyric = juce::MidiMessage::textMetaEvent (5, n.phonemes.isNotEmpty() && n.lyric.isEmpty() ? n.phonemes : n.lyric);
        lyric.setTimeStamp (on);
        seq.addEvent (lyric);
        seq.addEvent (juce::MidiMessage::noteOn (1, n.pitch, juce::jlimit (1.0f, 127.0f, n.velocity * 127.0f) / 127.0f), on);
        seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch), off - 1.0);
    }
    seq.updateMatchedPairs();
    return seq;
}

Score Score::fromMidi (const juce::MidiFile& file, int track)
{
    Score s;
    const double tpq = file.getTimeFormat() > 0 ? (double) file.getTimeFormat() : 960.0;
    int best = track;
    if (best < 0)
    {
        int most = -1;
        for (int t = 0; t < file.getNumTracks(); ++t)
        {
            int c = 0;
            for (auto* e : *file.getTrack (t)) c += e->message.isNoteOn() ? 1 : 0;
            if (c > most) { most = c; best = t; }
        }
    }
    if (best < 0 || best >= file.getNumTracks()) return s;
    juce::MidiMessageSequence seq (*file.getTrack (best));
    seq.updateMatchedPairs();

    // lyrics may be on any track
    std::vector<std::pair<double, juce::String>> lyrics;
    for (int t = 0; t < file.getNumTracks(); ++t)
        for (auto* e : *file.getTrack (t))
            if (e->message.isTextMetaEvent() && (e->message.getMetaEventType() == 5 || e->message.getMetaEventType() == 1))
            {
                auto txt = e->message.getTextFromTextMetaEvent().trim();
                if (txt.isNotEmpty() && ! txt.startsWithChar ('@')) lyrics.push_back ({ e->message.getTimeStamp(), txt });
            }

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        const auto* e = seq.getEventPointer (i);
        if (! e->message.isNoteOn()) continue;
        ScoreNote n;
        n.start = e->message.getTimeStamp() / tpq;
        const double off = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : e->message.getTimeStamp() + tpq;
        n.length = juce::jmax (1.0 / 32.0, (off - e->message.getTimeStamp()) / tpq);
        n.pitch = e->message.getNoteNumber();
        n.velocity = e->message.getFloatVelocity();
        for (const auto& [t, txt] : lyrics)
            if (std::abs (t - e->message.getTimeStamp()) < tpq / 32.0) { n.lyric = txt.removeCharacters ("\r\n/\\"); break; }
        s.addNote (n);
    }
    s.makeMonophonic();
    double last = 0.0;
    for (const auto& n : s.notes) last = juce::jmax (last, n.end());
    s.lengthBeats = juce::jmax (16.0, std::ceil (last / 4.0) * 4.0);
    return s;
}

//==============================================================================
std::vector<Syllable> noteSyllables (const ScoreNote& n, Language defaultLanguage)
{
    const auto lang = n.language >= 0 ? (Language) n.language : defaultLanguage;
    if (n.phonemes.isNotEmpty())
    {
        std::vector<Syllable> v;
        for (const auto& part : juce::StringArray::fromTokens (n.phonemes.removeCharacters ("[]"), "|.", ""))
            if (part.trim().isNotEmpty()) v.push_back (syllableFromPhonemes (part.trim()));
        if (! v.empty()) return v;
    }
    auto v = textToSyllables (n.lyric.isEmpty() ? juce::String ("la") : n.lyric, lang);
    if (v.empty()) v = textToSyllables ("a", Language::Japanese);
    return v;
}

std::unique_ptr<CompiledScore> compileScore (const Score& score, Language defaultLanguage)
{
    auto c = std::make_unique<CompiledScore>();
    c->lanes = score.lanes;
    c->lengthBeats = score.lengthBeats;
    c->startBeat = (score.startBar - 1) * 4.0;
    c->loop = score.loop;

    auto sorted = score.notes;
    std::stable_sort (sorted.begin(), sorted.end(), [] (const ScoreNote& a, const ScoreNote& b) { return a.start < b.start; });
    for (const auto& n : sorted)
    {
        const auto syls = noteSyllables (n, defaultLanguage);
        const int k = (int) juce::jmin<size_t> (syls.size(), 8);
        const double part = n.length / (double) k;
        for (int i = 0; i < k; ++i)
        {
            CompiledNote cn;
            cn.start = n.start + part * i;
            cn.end = n.start + part * (i + 1);
            cn.pitch = (float) n.pitch;
            cn.velocity = n.velocity;
            cn.syl = syls[(size_t) i];
            cn.portaMs = n.portaMs; cn.vibDepth = n.vibDepth; cn.vibRate = n.vibRate; cn.vibDelay = n.vibDelay;
            cn.consScale = n.consScale; cn.consStrength = n.consStrength;
            cn.sourceId = n.id;
            c->notes.push_back (cn);
        }
    }
    for (size_t i = 0; i < c->notes.size(); ++i)
    {
        auto& n = c->notes[i];
        if (i > 0)
        {
            auto& p = c->notes[i - 1];
            if (p.end > n.start) p.end = n.start;
            n.legatoFromPrev = n.start - p.end < 0.02;
            p.phraseEnd = ! n.legatoFromPrev;
        }
        n.phraseEnd = true;
        if (i + 1 < c->notes.size()) n.phraseEnd = c->notes[i + 1].start - n.end >= 0.02;
    }
    return c;
}

} // namespace talkcat
