#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/**
    Reads a YouTube live chat the way the chat window in the browser does (no API key, no daily
    limit): finds the live video of a channel, opens its chat and asks for new messages every second
    or two. Runs on its own thread; take() hands over what arrived.
*/
namespace ocean
{
class YouTubeChat : private juce::Thread
{
public:
    struct Message { juce::String id, authorId, authorName, text; };

    YouTubeChat();
    ~YouTubeChat() override;

    /** "@channel", a channel / video link, or an 11-character video id. */
    void start (const juce::String& channelOrVideo);
    void stop();
    bool isActive() const { return isThreadRunning(); }

    std::vector<Message> take();
    juce::String getStatus() const;
    int getMessageCount() const noexcept { return received.load(); }

    // pieces (public for the tests)
    static juce::String videoIdFrom (const juce::String& text);           // "" if the text has no video id
    static juce::String channelPathFrom (const juce::String& text);       // "@name" / "channel/UC..." / ""
    static juce::String jsonObjectAfter (const juce::String& page, const juce::String& marker);
    static juce::String quotedValueAfter (const juce::String& page, const juce::String& key);
    /** Reads one get_live_chat answer: new messages, the next continuation and how long to wait. */
    static bool parseChat (const juce::var& json, std::vector<Message>& out, juce::String& continuation, int& waitMs);
    static bool parseInitial (const juce::var& ytInitialData, juce::String& continuation, juce::StringArray& seenIds);

private:
    void run() override;
    void setStatus (const juce::String&);
    juce::String fetch (const juce::String& url, const juce::String& postJson = {});
    juce::String findLiveVideo (const juce::String& channelPath);
    bool waitOrQuit (int ms);

    juce::String input;
    mutable juce::CriticalSection lock;
    juce::String status { "not connected" };
    std::vector<Message> inbox;
    std::atomic<int> received { 0 };
};
} // namespace ocean
