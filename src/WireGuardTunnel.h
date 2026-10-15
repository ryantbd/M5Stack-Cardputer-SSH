#pragma once

#include <Arduino.h>
#include <lwip/netif.h>

#include "OperationCallbacks.h"
#include "VpnConfig.h"

struct tcpip_api_call_data;

namespace app {

// A single-peer WireGuard interface. Only the configured AllowedIPs subnet is
// routed through it (unless AllowedIPs is 0.0.0.0/0), so DNS, NTP and mDNS
// keep using Wi-Fi directly.
class WireGuardTunnel {
public:
    ~WireGuardTunnel();

    // The handshake carries a TAI64N timestamp from the system clock, and the
    // peer rejects one older than the last it accepted, so syncClock() must
    // succeed before start().
    bool start(const VpnConfigData& config, String& error);
    void stop();
    bool running() const { return netif_ != nullptr; }
    bool peerUp();

    static bool generatePrivateKey(char (&privateKey)[kWireGuardKeyCapacity]);
    static bool publicKeyFor(const char* privateKey, char (&publicKey)[kWireGuardKeyCapacity]);

private:
    struct StartRequest;
    static err_t startInNetworkThread(struct tcpip_api_call_data* call);
    static err_t stopInNetworkThread(struct tcpip_api_call_data* call);
    static err_t peerUpInNetworkThread(struct tcpip_api_call_data* call);

    struct netif netifStorage_ {};
    struct netif* netif_ = nullptr;
    struct netif* previousDefault_ = nullptr;
    uint8_t peerIndex_ = 0xFF;
    bool fullTunnel_ = false;
};

bool clockLooksValid();
bool syncClock(uint32_t timeoutMs, const OperationCallbacks& callbacks);

}  // namespace app
