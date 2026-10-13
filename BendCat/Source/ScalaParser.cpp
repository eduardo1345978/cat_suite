#include "ScalaParser.h"
#include <cmath>

namespace bendcat
{
bool ScalaParser::loadSclFile (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        lastError = "File not found: " + file.getFullPathName();
        return false;
    }
    return parse (file.loadFileAsString());
}

void ScalaParser::clear()
{
    degreesCents.clear();
    description.clear();
    rawText.clear();
    lastError.clear();
    loaded = false;
    tuningTableCents.fill (0.0f);
}

void ScalaParser::setReferenceNote (int note)
{
    referenceNote = juce::jlimit (0, 127, note);
    computeTable();
}

bool ScalaParser::parsePitch (const juce::String& raw, double& cents)
{
    const auto token = raw.trim().upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("\t", false, false);
    if (token.isEmpty())
        return false;

    if (token.containsChar ('.'))
    {
        cents = token.getDoubleValue();
        return true;
    }

    double num = 0.0, den = 1.0;
    if (token.containsChar ('/'))
    {
        num = token.upToFirstOccurrenceOf ("/", false, false).getDoubleValue();
        den = token.fromFirstOccurrenceOf ("/", false, false).getDoubleValue();
    }
    else
    {
        num = token.getDoubleValue();
    }

    if (num <= 0.0 || den <= 0.0)
        return false;

    cents = 1200.0 * std::log2 (num / den);
    return true;
}

bool ScalaParser::parse (const juce::String& text)
{
    juce::StringArray lines;
    lines.addLines (text);

    juce::String desc;
    int expected = -1, stage = 0;
    std::vector<double> degrees;

    for (auto line : lines)
    {
        if (line.startsWithChar ('!'))
            continue;

        if (stage == 0) { desc = line.trim(); stage = 1; continue; }
        if (stage == 1)
        {
            const auto t = line.trim();
            if (t.isEmpty()) continue;
            expected = t.getIntValue();
            stage = 2;
            continue;
        }

        if ((int) degrees.size() >= expected)
            break;

        if (line.trim().isEmpty())
            continue;

        double cents = 0.0;
        if (! parsePitch (line, cents))
        {
            lastError = "Invalid pitch line: " + line;
            return false;
        }
        degrees.push_back (cents);
    }

    if (expected <= 0 || (int) degrees.size() != expected)
    {
        lastError = "Expected " + juce::String (expected) + " pitches, found " + juce::String ((int) degrees.size());
        return false;
    }

    degreesCents = std::move (degrees);
    description = desc;
    rawText = text;
    lastError.clear();
    loaded = true;
    computeTable();
    return true;
}

void ScalaParser::computeTable()
{
    tuningTableCents.fill (0.0f);
    if (! loaded || degreesCents.empty())
        return;

    const int n = (int) degreesCents.size();
    const double period = degreesCents.back();

    for (int note = 0; note < 128; ++note)
    {
        const int steps = note - referenceNote;
        const int octave = (int) std::floor ((double) steps / n);
        const int degree = steps - octave * n;
        const double cents = octave * period + (degree == 0 ? 0.0 : degreesCents[(size_t) (degree - 1)]);
        tuningTableCents[(size_t) note] = (float) (cents - steps * 100.0);
    }
}

} // namespace bendcat
