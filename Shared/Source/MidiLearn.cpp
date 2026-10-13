#include "MidiLearn.h"

namespace cat
{
MidiLearnManager::MidiLearnManager (juce::AudioProcessor& p) : processor (p)
{
    for (auto& c : ccToParam)
        c.store (-1);

    for (auto* param : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            params.add (ranged);
            paramIDs.add (ranged->getParameterID());
        }

    scratch.ensureSize (4096);
    startTimerHz (30);
}

MidiLearnManager::~MidiLearnManager()
{
    stopTimer();
}

int MidiLearnManager::indexOf (const juce::String& paramID) const
{
    return paramIDs.indexOf (paramID);
}

void MidiLearnManager::startLearning (const juce::String& paramID)
{
    learningParam.store (indexOf (paramID));
}

void MidiLearnManager::cancelLearning()
{
    learningParam.store (-1);
}

bool MidiLearnManager::isLearning (const juce::String& paramID) const
{
    const int idx = learningParam.load();
    return idx >= 0 && idx == indexOf (paramID);
}

void MidiLearnManager::clearMapping (const juce::String& paramID)
{
    const int idx = indexOf (paramID);
    for (auto& c : ccToParam)
        if (c.load() == idx)
            c.store (-1);
}

int MidiLearnManager::getCCForParam (const juce::String& paramID) const
{
    const int idx = indexOf (paramID);
    if (idx < 0)
        return -1;

    for (int cc = 0; cc < 128; ++cc)
        if (ccToParam[(size_t) cc].load() == idx)
            return cc;

    return -1;
}

void MidiLearnManager::processMidi (juce::MidiBuffer& midi)
{
    bool anyConsumed = false;
    scratch.clear();

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();

        if (msg.isController() && msg.getControllerNumber() < 120)
        {
            const int cc = msg.getControllerNumber();
            const int learning = learningParam.load();

            if (learning >= 0)
            {
                for (auto& c : ccToParam) // one CC per parameter
                    if (c.load() == learning)
                        c.store (-1);

                ccToParam[(size_t) cc].store (learning);
                learningParam.store (-1);
                anyConsumed = true;
                continue;
            }

            const int target = ccToParam[(size_t) cc].load();
            if (target >= 0)
            {
                const auto scope = fifo.write (1);
                if (scope.blockSize1 > 0)
                    fifoData[(size_t) scope.startIndex1] = { target, (float) msg.getControllerValue() / 127.0f };
                else if (scope.blockSize2 > 0)
                    fifoData[(size_t) scope.startIndex2] = { target, (float) msg.getControllerValue() / 127.0f };

                anyConsumed = true;
                continue;
            }
        }

        scratch.addEvent (msg, meta.samplePosition);
    }

    if (anyConsumed)
        midi.swapWith (scratch);
}

void MidiLearnManager::timerCallback()
{
    const auto scope = fifo.read (fifo.getNumReady());

    auto apply = [this] (int start, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            const auto [index, value] = fifoData[(size_t) (start + i)];
            if (auto* p = params[index])
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (value);
                p->endChangeGesture();
            }
        }
    };

    apply (scope.startIndex1, scope.blockSize1);
    apply (scope.startIndex2, scope.blockSize2);
}

juce::ValueTree MidiLearnManager::toValueTree() const
{
    juce::ValueTree tree (treeId);

    for (int cc = 0; cc < 128; ++cc)
    {
        const int idx = ccToParam[(size_t) cc].load();
        if (idx >= 0 && idx < paramIDs.size())
        {
            juce::ValueTree map ("MAP");
            map.setProperty ("cc", cc, nullptr);
            map.setProperty ("param", paramIDs[idx], nullptr);
            tree.appendChild (map, nullptr);
        }
    }

    return tree;
}

void MidiLearnManager::fromValueTree (const juce::ValueTree& tree)
{
    for (auto& c : ccToParam)
        c.store (-1);

    for (const auto& map : tree)
    {
        const int cc = map.getProperty ("cc", -1);
        const int idx = indexOf (map.getProperty ("param").toString());
        if (juce::isPositiveAndBelow (cc, 128) && idx >= 0)
            ccToParam[(size_t) cc].store (idx);
    }
}

} // namespace cat
