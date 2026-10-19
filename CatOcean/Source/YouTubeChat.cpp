#include "YouTubeChat.h"

namespace ocean
{
namespace
{
    const char* const userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36";

    bool isIdChar (juce::juce_wchar c) { return juce::CharacterFunctions::isLetterOrDigit (c) || c == '-' || c == '_'; }

    juce::String idAfter (const juce::String& text, const juce::String& marker)
    {
        const int at = text.indexOf (marker);
        if (at < 0) return {};
        juce::String id;
        for (int i = at + marker.length(); i < text.length() && isIdChar (text[i]) && id.length() < 11; ++i) id << juce::String::charToString (text[i]);
        return id.length() == 11 ? id : juce::String();
    }

    juce::String runsText (const juce::var& message)
    {
        juce::String s;
        if (auto* runs = message["runs"].getArray())
            for (auto& run : *runs)
            {
                if (run.hasProperty ("text")) s << run["text"].toString();
                else if (run.hasProperty ("emoji"))
                {
                    const auto& e = run["emoji"];
                    if (auto* sc = e["shortcuts"].getArray(); sc != nullptr && ! sc->isEmpty()) s << (*sc)[0].toString();
                }
            }
        else if (message.hasProperty ("simpleText")) s = message["simpleText"].toString();
        return s;
    }

    bool continuationOf (const juce::var& continuations, juce::String& cont, int* waitMs)
    {
        if (auto* arr = continuations.getArray())
            for (auto& c : *arr)
                if (auto* obj = c.getDynamicObject())
                    for (auto& p : obj->getProperties())
                        if (p.value.hasProperty ("continuation"))
                        {
                            cont = p.value["continuation"].toString();
                            if (waitMs != nullptr && p.value.hasProperty ("timeoutMs")) *waitMs = (int) p.value["timeoutMs"];
                            return cont.isNotEmpty();
                        }
        return false;
    }
}

YouTubeChat::YouTubeChat() : juce::Thread ("YouTube chat") {}
YouTubeChat::~YouTubeChat() { stop(); }

void YouTubeChat::start (const juce::String& channelOrVideo)
{
    stop();
    input = channelOrVideo.trim();
    startThread();
}

void YouTubeChat::stop()
{
    signalThreadShouldExit();
    notify();
    stopThread (20000);
    setStatus ("not connected");
}

std::vector<YouTubeChat::Message> YouTubeChat::take()
{
    const juce::ScopedLock sl (lock);
    std::vector<Message> out;
    out.swap (inbox);
    return out;
}

juce::String YouTubeChat::getStatus() const { const juce::ScopedLock sl (lock); return status; }
void YouTubeChat::setStatus (const juce::String& s) { const juce::ScopedLock sl (lock); status = s; }
bool YouTubeChat::waitOrQuit (int ms) { wait (ms); return ! threadShouldExit(); }

//==============================================================================
juce::String YouTubeChat::videoIdFrom (const juce::String& t)
{
    for (auto* marker : { "watch?v=", "&v=", "youtu.be/", "/live/", "/shorts/", "/embed/", "live_chat?v=" })
    {
        const auto id = idAfter (t, marker);
        if (id.isNotEmpty()) return id;
    }
    const auto s = t.trim();
    if (s.length() == 11 && ! s.startsWithChar ('@') && s.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"))
        return s;
    return {};
}

juce::String YouTubeChat::channelPathFrom (const juce::String& t)
{
    const auto s = t.trim();
    const int at = s.indexOfChar ('@');
    if (at >= 0)
    {
        juce::String h = "@";
        for (int i = at + 1; i < s.length() && (isIdChar (s[i]) || s[i] == '.'); ++i) h << juce::String::charToString (s[i]);
        return h.length() > 1 ? h : juce::String();
    }
    const int ch = s.indexOf ("channel/");
    if (ch >= 0)
    {
        juce::String id;
        for (int i = ch + 8; i < s.length() && isIdChar (s[i]); ++i) id << juce::String::charToString (s[i]);
        return id.isNotEmpty() ? "channel/" + id : juce::String();
    }
    if (s.startsWith ("UC") && s.length() == 24) return "channel/" + s;
    return {};
}

juce::String YouTubeChat::jsonObjectAfter (const juce::String& page, const juce::String& marker)
{
    const int m = page.indexOf (marker);
    if (m < 0) return {};
    const int start = page.indexOfChar (m, '{');
    if (start < 0) return {};
    int depth = 0;
    bool inString = false, escape = false;
    auto p = page.getCharPointer() + start;
    int i = start;
    for (; ! p.isEmpty(); ++p, ++i)
    {
        const auto c = *p;
        if (inString)
        {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '{') ++depth;
        else if (c == '}' && --depth == 0) return page.substring (start, i + 1);
    }
    return {};
}

juce::String YouTubeChat::quotedValueAfter (const juce::String& page, const juce::String& key)
{
    const juce::String marker = "\"" + key + "\":\"";
    const int at = page.indexOf (marker);
    if (at < 0) return {};
    const int from = at + marker.length();
    const int to = page.indexOfChar (from, '"');
    return to > from ? page.substring (from, to) : juce::String();
}

bool YouTubeChat::parseInitial (const juce::var& data, juce::String& continuation, juce::StringArray& seenIds)
{
    const auto& chat = data["contents"]["liveChatRenderer"];
    if (! chat.isObject()) return false;
    if (auto* actions = chat["actions"].getArray())
        for (auto& a : *actions)
        {
            const auto& item = a["addChatItemAction"]["item"];
            for (auto* r : { "liveChatTextMessageRenderer", "liveChatPaidMessageRenderer" })
                if (item[r].isObject()) seenIds.add (item[r]["id"].toString());
        }
    return continuationOf (chat["continuations"], continuation, nullptr);
}

bool YouTubeChat::parseChat (const juce::var& json, std::vector<Message>& out, juce::String& continuation, int& waitMs)
{
    const auto& live = json["continuationContents"]["liveChatContinuation"];
    if (! live.isObject()) return false;
    if (auto* actions = live["actions"].getArray())
        for (auto& a : *actions)
        {
            const auto& item = a["addChatItemAction"]["item"];
            for (auto* r : { "liveChatTextMessageRenderer", "liveChatPaidMessageRenderer" })
            {
                const auto& m = item[r];
                if (! m.isObject()) continue;
                Message msg;
                msg.id = m["id"].toString();
                msg.authorId = m["authorExternalChannelId"].toString();
                msg.authorName = m["authorName"]["simpleText"].toString();
                msg.text = runsText (m["message"]);
                if (auto* badges = m["authorBadges"].getArray())
                    for (auto& b : *badges)
                    {
                        const auto type = b["liveChatAuthorBadgeRenderer"]["icon"]["iconType"].toString();
                        if (type == "MODERATOR" || type == "OWNER") msg.moderator = true;
                    }
                if (msg.authorId.isNotEmpty() && msg.text.isNotEmpty()) out.push_back (msg);
            }
        }
    waitMs = 1500;
    juce::String next;
    if (continuationOf (live["continuations"], next, &waitMs)) continuation = next;
    else continuation = {};
    return true;
}

//==============================================================================
juce::String YouTubeChat::fetch (const juce::String& url, const juce::String& postJson)
{
    juce::String headers;
    headers << "User-Agent: " << userAgent << "\r\n"
            << "Accept-Language: en-US,en;q=0.9\r\n"
            << "Cookie: CONSENT=YES+cb; SOCS=CAI\r\n";   // skip the EU cookie page
    juce::URL u (url);
    auto params = juce::URL::ParameterHandling::inAddress;
    if (postJson.isNotEmpty())
    {
        u = u.withPOSTData (postJson);
        params = juce::URL::ParameterHandling::inPostData;
        headers << "Content-Type: application/json\r\n";
    }
    int statusCode = 0;
    auto stream = u.createInputStream (juce::URL::InputStreamOptions (params)
                                           .withExtraHeaders (headers)
                                           .withConnectionTimeoutMs (15000)
                                           .withStatusCode (&statusCode)
                                           .withNumRedirectsToFollow (5));
    if (stream == nullptr || (statusCode != 0 && statusCode >= 400)) return {};
    return stream->readEntireStreamAsString();
}

int YouTubeChat::parseSubscribers (const juce::String& page)
{
    // "1.23K subscribers", "987 subscribers", "1,234 subscribers": the number before the word
    int from = page.indexOf ("subscriberCountText");
    if (from < 0) from = 0;
    const int at = page.indexOf (from, " subscriber");
    if (at < 0) return -1;
    int start = at;
    while (start > 0 && (juce::CharacterFunctions::isDigit (page[start - 1]) || page[start - 1] == '.' || page[start - 1] == ','
                          || page[start - 1] == 'K' || page[start - 1] == 'M')) --start;
    auto token = page.substring (start, at).removeCharacters (",");
    if (token.isEmpty()) return -1;
    double mult = 1.0;
    if (token.endsWithChar ('K')) { mult = 1000.0; token = token.dropLastCharacters (1); }
    else if (token.endsWithChar ('M')) { mult = 1000000.0; token = token.dropLastCharacters (1); }
    if (token.isEmpty() || ! token.containsOnly ("0123456789.")) return -1;
    return (int) std::lround (token.getDoubleValue() * mult);
}

void YouTubeChat::checkSubscribers (const juce::String& channelPath)
{
    if (channelPath.isEmpty()) return;
    const auto now = juce::Time::getMillisecondCounter();
    if (lastSubCheck != 0 && now - lastSubCheck < 60000) return;
    lastSubCheck = now;
    const int n = parseSubscribers (fetch ("https://www.youtube.com/" + channelPath));
    if (n >= 0) subscribers = n;
}

juce::String YouTubeChat::findLiveVideo (const juce::String& channelPath)
{
    const auto page = fetch ("https://www.youtube.com/" + channelPath + "/live");
    if (page.isEmpty()) return {};
    auto id = idAfter (page, "<link rel=\"canonical\" href=\"https://www.youtube.com/watch?v=");
    return id;
}

void YouTubeChat::run()
{
    while (! threadShouldExit())
    {
        auto video = videoIdFrom (input);
        const auto channel = channelPathFrom (input);
        if (video.isEmpty() && channel.isEmpty()) { setStatus ("type your channel (@name) or the link of the live video"); return; }
        if (video.isEmpty())
        {
            setStatus ("looking for a live stream on " + channel + " ...");
            video = findLiveVideo (channel);
            if (video.isEmpty())
            {
                setStatus (channel + " is not live yet - checking again in 30 s");
                if (! waitOrQuit (30000)) return;
                continue;
            }
        }
        setStatus ("opening the chat of video " + video + " ...");
        const auto page = fetch ("https://www.youtube.com/live_chat?is_popout=1&v=" + video);
        if (page.isEmpty())
        {
            setStatus ("can't reach YouTube - trying again in 15 s");
            if (! waitOrQuit (15000)) return;
            continue;
        }
        juce::String cont;
        juce::StringArray seen;
        const auto data = juce::JSON::parse (jsonObjectAfter (page, "ytInitialData"));
        if (! parseInitial (data, cont, seen))
        {
            setStatus ("video " + video + " has no live chat yet (is the stream live?) - trying again in 30 s");
            if (! waitOrQuit (30000)) return;
            continue;
        }
        auto version = quotedValueAfter (page, "INNERTUBE_CLIENT_VERSION");
        if (version.isEmpty()) version = "2.20241010.00.00";
        const auto key = quotedValueAfter (page, "INNERTUBE_API_KEY");
        const juce::String endpoint = "https://www.youtube.com/youtubei/v1/live_chat/get_live_chat?prettyPrint=false"
                                    + (key.isNotEmpty() ? "&key=" + key : juce::String());
        setStatus ("connected to the chat of video " + video);

        int errors = 0;
        while (! threadShouldExit() && cont.isNotEmpty())
        {
            juce::DynamicObject::Ptr client = new juce::DynamicObject(), context = new juce::DynamicObject(), body = new juce::DynamicObject();
            client->setProperty ("clientName", "WEB");
            client->setProperty ("clientVersion", version);
            client->setProperty ("hl", "en");
            context->setProperty ("client", juce::var (client.get()));
            body->setProperty ("context", juce::var (context.get()));
            body->setProperty ("continuation", cont);
            const auto answer = fetch (endpoint, juce::JSON::toString (juce::var (body.get()), true));

            std::vector<Message> messages;
            int waitMs = 1500;
            juce::String next = cont;
            if (answer.isEmpty() || ! parseChat (juce::JSON::parse (answer), messages, next, waitMs))
            {
                if (++errors > 5) break;
                if (! waitOrQuit (3000)) return;
                continue;
            }
            errors = 0;
            {
                const juce::ScopedLock sl (lock);
                for (auto& m : messages)
                {
                    if (seen.contains (m.id)) continue;
                    seen.add (m.id);
                    inbox.push_back (m);
                    ++received;
                }
                if (inbox.size() > 2000) inbox.erase (inbox.begin(), inbox.begin() + 1000);
            }
            if (seen.size() > 600) seen.removeRange (0, 300);
            cont = next;
            checkSubscribers (channel);
            if (! waitOrQuit (juce::jlimit (1000, 5000, waitMs))) return;
        }
        setStatus ("lost the chat connection - reconnecting ...");
        if (! waitOrQuit (5000)) return;
    }
}
} // namespace ocean
