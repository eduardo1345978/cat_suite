#include "BoxLink.h"

namespace catbox
{
//==============================================================================
LinkServer::LinkServer (BoxController& b, int port, bool advertise, const juce::String& bindAddress) : box (b)
{
    listening = beginWaitingForSocket (port, bindAddress);
    if (listening && advertise)
        advertiser = std::make_unique<juce::NetworkServiceDiscovery::Advertiser> (linkServiceId, "Cat Box", discoveryPort, port);
    startTimerHz (15);
}

LinkServer::~LinkServer()
{
    stopTimer();
    advertiser.reset();
    stop();
    const juce::ScopedLock sl (lock);
    clients.clear();
}

int LinkServer::numClients() const
{
    const juce::ScopedLock sl (lock);
    int n = 0;
    for (auto& c : clients) if (c->made.load() && ! c->lost.load()) ++n;
    return n;
}

juce::String LinkServer::ownAddress()
{
    for (const auto& a : juce::IPAddress::getAllAddresses (false))
        if (a != juce::IPAddress::local (false) && ! a.toString().startsWith ("169.254")) return a.toString();
    return {};
}

juce::InterprocessConnection* LinkServer::createConnectionObject()
{
    // called on the server's thread; the connection talks on the message thread
    auto c = std::make_unique<LinkConnection>();
    auto* raw = c.get();
    c->onMessage = [this, raw] (const juce::var& m)
    {
        box.handleLinkMessage (m, [raw] (const juce::var& reply) { raw->send (reply); });
        if (m["t"].toString() == "hello") raw->needsState = false;
    };
    const juce::ScopedLock sl (lock);
    clients.push_back (std::move (c));
    return raw;
}

void LinkServer::timerCallback()
{
    const juce::ScopedLock sl (lock);
    // forget PCs that went away
    clients.erase (std::remove_if (clients.begin(), clients.end(), [] (const std::unique_ptr<LinkConnection>& c) { return c->lost.load(); }),
                   clients.end());
    bool any = false;
    for (auto& c : clients) any = any || (c->made.load() && c->isConnected());
    if (! any) return;

    const auto state = box.makeState();
    const auto stateText = juce::JSON::toString (state, true);
    const auto live = box.makeLive();
    for (auto& c : clients)
    {
        if (! c->made.load() || ! c->isConnected()) continue;
        if (c->needsState.load() || stateText != c->lastStateSent || ++ticks % 30 == 0)
        {
            c->send (state);
            c->lastStateSent = stateText;
            c->needsState = false;
        }
        c->send (live);
    }
}

//==============================================================================
LinkClient::LinkClient (BoxController& b, bool listenForBoxes) : box (b)
{
    if (listenForBoxes)
    {
        services = std::make_unique<juce::NetworkServiceDiscovery::AvailableServiceList> (linkServiceId, discoveryPort);
        services->onChange = [this] { if (onChange) onChange(); };
    }
    startTimerHz (2);
}

LinkClient::~LinkClient()
{
    stopTimer();
    disconnect();
    services.reset();
}

std::vector<LinkClient::Found> LinkClient::found() const
{
    std::vector<Found> out;
    if (services != nullptr)
        for (const auto& s : services->getServices())
            out.push_back ({ s.description, s.address.toString(), s.port });
    return out;
}

bool LinkClient::connect (const juce::String& h, int port)
{
    disconnect();
    conn = std::make_unique<LinkConnection>();
    conn->onMessage = [this] (const juce::var& m)
    {
        const auto t = m["t"].toString();
        if (t == "state") box.applyState (m);
        else if (t == "live") box.applyLive (m);
        else if (t == "preset")
        {
            const int pg = (int) m["page"], sl = (int) m["slot"];
            for (auto it = waiting.begin(); it != waiting.end(); ++it)
                if (it->page == pg && it->slot == sl)
                {
                    auto done = std::move (it->done);
                    waiting.erase (it);
                    if (done) done (m["data"]);
                    break;
                }
        }
    };
    conn->onLost = [this]
    {
        // tidy up later, not inside the connection's own callback
        juce::WeakReference<LinkClient> weak (this);
        juce::MessageManager::callAsync ([weak] { if (auto* self = weak.get()) if (self->conn != nullptr && self->conn->lost.load()) self->lostConnection(); });
    };
    if (! conn->connectToSocket (h, port, 2500))
    {
        conn.reset();
        return false;
    }
    host = h;
    box.setRemoteSink ([this] (const juce::var& v) { send (v); });
    send (juce::JSON::parse ("{\"t\":\"hello\"}"));
    if (onChange) onChange();
    return true;
}

void LinkClient::disconnect()
{
    if (conn == nullptr) return;
    conn->onLost = nullptr;
    conn.reset();
    lostConnection();
}

void LinkClient::lostConnection()
{
    conn.reset();
    host.clear();
    waiting.clear();
    if (box.isRemote()) box.setRemoteSink (nullptr);
    if (onChange) onChange();
}

bool LinkClient::isConnected() const
{
    return conn != nullptr && conn->isConnected();
}

void LinkClient::send (const juce::var& v)
{
    if (isConnected()) conn->send (v);
}

void LinkClient::requestPreset (int page, int slotIndex, std::function<void (const juce::var&)> done)
{
    if (! isConnected()) { if (done) done ({}); return; }
    waiting.push_back ({ page, slotIndex, std::move (done) });
    auto* o = new juce::DynamicObject();
    o->setProperty ("t", "getPreset");
    o->setProperty ("page", page);
    o->setProperty ("slot", slotIndex);
    send (juce::var (o));
}

void LinkClient::timerCallback()
{
    const auto n = found().size();
    if (n != lastFoundCount) { lastFoundCount = n; if (onChange) onChange(); }
}

} // namespace catbox
