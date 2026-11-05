#pragma once

#include <juce_events/juce_events.h>
#include "Controller.h"
#include <memory>
#include <vector>
#include <algorithm>
#include <atomic>

/**
    The link between the box and the Cat Box app on a PC, over the home network (WiFi or cable).

    The box (Raspberry Pi) runs a LinkServer: it answers on TCP port 50650 and tells the network
    it is there (UDP broadcast on 50651). The PC app runs a LinkClient: it finds boxes, connects,
    and from then on its knobs, pads and buttons play the box; the PC screen mirrors the box and
    the PC screen shows what the box does.

    Messages are small JSON objects with a "t" (type):
      PC -> box: hello (with the link code), knob, reset, button, pad, key, slider, joy, track, mode
      box -> PC: state (the project and the screen), live (what plays now + the last 20 ms of sound), denied

    The link code: the box makes a 4-digit code when it starts and shows it with its address (PROJECT page).
    A PC must send it in its hello before the box listens to anything it says; a wrong code is refused and
    the connection closed, and after 5 wrong codes the box takes no codes for 30 s. So nobody else on the
    same WiFi can play or reset the box.
*/
namespace catbox
{
constexpr int linkPort = 50650, discoveryPort = 50651;
inline const char* const linkServiceId = "catbox-link-2";

class LinkConnection : public juce::InterprocessConnection
{
public:
    LinkConnection() : juce::InterprocessConnection (true, 0xca7b0c51) {}
    ~LinkConnection() override { disconnect(); }

    std::function<void()> onMade, onLost;
    std::function<void (const juce::var&)> onMessage;

    void send (const juce::var& v)
    {
        const auto s = juce::JSON::toString (v, true);
        sendMessage (juce::MemoryBlock (s.toRawUTF8(), s.getNumBytesAsUTF8()));
    }

    void connectionMade() override   { made = true; if (onMade) onMade(); }
    void connectionLost() override   { lost = true; if (onLost) onLost(); }
    void messageReceived (const juce::MemoryBlock& m) override { if (onMessage) onMessage (juce::JSON::parse (m.toString())); }

    std::atomic<bool> made { false }, lost { false }, needsState { true };
    std::atomic<bool> paired { false }, kick { false };        // gave the right code / to be closed
    juce::uint32 openedAt = juce::Time::getMillisecondCounter();
    juce::String lastStateSent;
};

//==============================================================================
/** The box side: answers PCs and sends them its state. */
class LinkServer : private juce::InterprocessConnectionServer, private juce::Timer
{
public:
    LinkServer (Controller&, int port = linkPort, bool advertise = true, const juce::String& bindAddress = {});
    ~LinkServer() override;

    bool isListening() const noexcept { return listening; }
    int numClients() const;
    static juce::String ownAddress();   // this box's address on the network (for the screen)
    /** The link code a PC must give (4 digits, new every time the box starts). */
    juce::String code() const { return pairCode; }

private:
    juce::InterprocessConnection* createConnectionObject() override;
    void timerCallback() override;

    Controller& box;
    juce::CriticalSection lock;
    std::vector<std::unique_ptr<LinkConnection>> clients;
    std::unique_ptr<juce::NetworkServiceDiscovery::Advertiser> advertiser;
    bool listening = false;
    int ticks = 0;
    juce::String pairCode;
    int wrongCodes = 0;
    juce::uint32 lockedUntil = 0;
};

//==============================================================================
/** The PC side: finds boxes and plays one of them. */
class LinkClient : private juce::Timer
{
public:
    explicit LinkClient (Controller&, bool listenForBoxes = true);
    ~LinkClient() override;

    struct Found { juce::String name, address; int port = linkPort; };
    std::vector<Found> found() const;

    /** Connects and gives the box its link code. false = nobody answered (a wrong code shows up as wasDenied()). */
    bool connect (const juce::String& host, int port = linkPort, const juce::String& code = {});
    bool wasDenied() const noexcept { return denied; }
    void disconnect();
    bool isConnected() const;
    juce::String connectedTo() const { return isConnected() ? host : juce::String(); }

    void send (const juce::var&);
    /** Asks the box for one preset (page -1 = scene); done gets its JSON (or void when the slot is empty). */
    void requestPreset (int page, int slotIndex, std::function<void (const juce::var&)> done);

    std::function<void()> onChange;   // connected / disconnected / boxes found

private:
    void timerCallback() override;
    void lostConnection();

    Controller& box;
    std::unique_ptr<LinkConnection> conn;
    std::unique_ptr<juce::NetworkServiceDiscovery::AvailableServiceList> services;
    juce::String host;
    struct Waiting { int page, slot; std::function<void (const juce::var&)> done; };
    std::vector<Waiting> waiting;   // preset requests
    size_t lastFoundCount = 0;
    bool denied = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (LinkClient)
};
} // namespace catbox
