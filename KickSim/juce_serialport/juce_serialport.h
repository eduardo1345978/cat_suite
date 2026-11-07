/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 juce_serialport
  vendor:             Cat Suite
  version:            0.0.1
  name:               Serial port stand-in
  description:        A do-nothing stand-in for the serial port module the KICK release could not include.
                      The simulator has no front-panel board to talk to, so no port is ever found.
  license:            MIT
  dependencies:       juce_core, juce_events

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/
#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>

class SerialPortConfig
{
public:
    enum SerialPortParity { SERIALPORT_PARITY_NONE, SERIALPORT_PARITY_ODD, SERIALPORT_PARITY_EVEN, SERIALPORT_PARITY_SPACE, SERIALPORT_PARITY_MARK };
    enum SerialPortStopBits { STOPBITS_1, STOPBITS_1ANDHALF, STOPBITS_2 };
    enum SerialPortFlowControl { FLOWCONTROL_NONE, FLOWCONTROL_HARDWARE, FLOWCONTROL_XONXOFF };
    SerialPortConfig () = default;
    SerialPortConfig (juce::uint32 bps_, juce::uint32 databits_, SerialPortParity parity_, SerialPortStopBits stopbits_, SerialPortFlowControl flowcontrol_)
        : bps (bps_), databits (databits_), parity (parity_), stopbits (stopbits_), flowcontrol (flowcontrol_) {}
    juce::uint32 bps = 9600, databits = 8;
    SerialPortParity parity = SERIALPORT_PARITY_NONE;
    SerialPortStopBits stopbits = STOPBITS_1;
    SerialPortFlowControl flowcontrol = FLOWCONTROL_NONE;
};

class SerialPort
{
public:
    using DebugFunction = std::function<void (juce::String, juce::String)>;
    explicit SerialPort (DebugFunction = nullptr) {}
    SerialPort (const juce::String&, const SerialPortConfig&, DebugFunction = nullptr) {}
    virtual ~SerialPort () = default;
    bool open (const juce::String&) { return false; }
    void close () {}
    bool exists () const { return false; }
    bool setConfig (const SerialPortConfig&) { return false; }
    bool getConfig (SerialPortConfig&) { return false; }
    static juce::StringPairArray getSerialPortPaths () { return {}; }
};

class SerialPortInputStream : public juce::InputStream, public juce::ChangeBroadcaster
{
public:
    enum { NOTIFY_OFF = 0, NOTIFY_ON_CHAR, NOTIFY_ALWAYS };
    explicit SerialPortInputStream (SerialPort*) {}
    void setNotify (int, char = 0) {}
    bool canReadLine () { return false; }
    juce::int64 getTotalLength () override { return 0; }
    bool isExhausted () override { return true; }
    int read (void*, int) override { return 0; }
    juce::int64 getPosition () override { return 0; }
    bool setPosition (juce::int64) override { return false; }
};

class SerialPortOutputStream : public juce::OutputStream
{
public:
    explicit SerialPortOutputStream (SerialPort*) {}
    void flush () override {}
    bool setPosition (juce::int64) override { return false; }
    juce::int64 getPosition () override { return 0; }
    bool write (const void*, size_t) override { return true; }
};
