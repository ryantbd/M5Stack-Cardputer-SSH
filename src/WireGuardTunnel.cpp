#include "WireGuardTunnel.h"

#include <WiFi.h>
#include <esp_random.h>
#include <lwip/ip.h>
#include <lwip/priv/tcpip_priv.h>
#include <sys/time.h>
#include <time.h>

#include "AppConfig.h"

extern "C" {
#include "crypto.h"
#include "wireguard-platform.h"
#include "wireguard.h"
#include "wireguardif.h"
}

namespace app {

namespace {

constexpr time_t kEarliestPlausibleTime = 1704067200;  // 2024-01-01T00:00:00Z
constexpr size_t kWireGuardKeyBytes = 32;

uint32_t prefixToMask(uint8_t prefix) {
    return prefix == 0 ? 0U : 0xFFFFFFFFU << (32U - prefix);
}

ip4_addr_t toIp4(const uint8_t (&address)[4]) {
    ip4_addr_t result;
    IP4_ADDR(&result, address[0], address[1], address[2], address[3]);
    return result;
}

ip4_addr_t maskToIp4(uint8_t prefix) {
    const uint32_t mask = prefixToMask(prefix);
    ip4_addr_t result;
    IP4_ADDR(&result, (mask >> 24) & 0xFF, (mask >> 16) & 0xFF, (mask >> 8) & 0xFF, mask & 0xFF);
    return result;
}

bool addressInSubnet(const uint8_t (&address)[4], const uint8_t (&subnet)[4], uint8_t prefix) {
    const uint32_t mask = prefixToMask(prefix);
    const uint32_t value = (static_cast<uint32_t>(address[0]) << 24) | (address[1] << 16) | (address[2] << 8) | address[3];
    const uint32_t network = (static_cast<uint32_t>(subnet[0]) << 24) | (subnet[1] << 16) | (subnet[2] << 8) | subnet[3];
    return (value & mask) == (network & mask);
}

}  // namespace

struct WireGuardTunnel::StartRequest {
    struct tcpip_api_call_data call;
    WireGuardTunnel* tunnel;
    const VpnConfigData* config;
    ip4_addr_t endpoint;
    uint8_t presharedKey[kWireGuardKeyBytes];
    bool hasPresharedKey;
    const char* error;
};

namespace {

struct TunnelCall {
    struct tcpip_api_call_data call;
    WireGuardTunnel* tunnel;
    bool result;
};

}  // namespace

WireGuardTunnel::~WireGuardTunnel() {
    stop();
}

bool WireGuardTunnel::start(const VpnConfigData& config, String& error) {
    stop();
    error = "";
    if (!vpnConfigReady(config)) {
        error = "VPN config incomplete";
        return false;
    }
    if (config.allowedPrefix != 0 && !addressInSubnet(config.address, config.allowedIp, config.allowedPrefix)) {
        error = "AllowedIPs must contain Address";
        return false;
    }
    if (!clockLooksValid()) {
        error = "Clock not set";
        return false;
    }

    // DNS needs the network thread, so resolve before handing over to it.
    uint8_t endpointBytes[4]{};
    if (!parseIpv4(config.endpointHost, endpointBytes)) {
        IPAddress resolved;
        if (WiFi.hostByName(config.endpointHost, resolved) != 1) {
            error = "Cannot resolve VPN endpoint";
            return false;
        }
        for (uint8_t index = 0; index < 4; ++index) endpointBytes[index] = resolved[index];
    }

    StartRequest request{};
    request.tunnel = this;
    request.config = &config;
    request.endpoint = toIp4(endpointBytes);
    if (config.presharedKey[0] != '\0') {
        size_t length = sizeof(request.presharedKey);
        request.hasPresharedKey = wireguard_base64_decode(config.presharedKey, request.presharedKey, &length) &&
                                  length == kWireGuardKeyBytes;
        if (!request.hasPresharedKey) {
            error = "Invalid preshared key";
            return false;
        }
    }

    wireguard_platform_init();
    const err_t result = tcpip_api_call(startInNetworkThread, &request.call);
    secureClear(request.presharedKey, sizeof(request.presharedKey));
    if (result != ERR_OK || netif_ == nullptr) {
        error = request.error != nullptr ? request.error : "Cannot start VPN";
        return false;
    }
    return true;
}

err_t WireGuardTunnel::startInNetworkThread(struct tcpip_api_call_data* call) {
    StartRequest* request = reinterpret_cast<StartRequest*>(call);
    WireGuardTunnel* tunnel = request->tunnel;
    const VpnConfigData& config = *request->config;

    struct wireguardif_init_data init {};
    init.private_key = config.privateKey;
    init.listen_port = 0;  // Any free port; roaming clients don't need a fixed one.
    init.bind_netif = nullptr;

    const ip4_addr_t address = toIp4(config.address);
    const ip4_addr_t netmask = maskToIp4(config.allowedPrefix);
    ip4_addr_t gateway;
    ip4_addr_set_zero(&gateway);

    struct netif* added = netif_add(&tunnel->netifStorage_, &address, &netmask, &gateway, &init,
                                    &wireguardif_init, &ip_input);
    if (added == nullptr) {
        request->error = "Cannot create VPN interface";
        return ERR_IF;
    }
    netif_set_up(added);

    struct wireguardif_peer peer;
    wireguardif_peer_init(&peer);
    peer.public_key = config.peerPublicKey;
    peer.preshared_key = request->hasPresharedKey ? request->presharedKey : nullptr;
    ip_addr_copy_from_ip4(peer.allowed_ip, toIp4(config.allowedIp));
    ip_addr_copy_from_ip4(peer.allowed_mask, maskToIp4(config.allowedPrefix));
    ip_addr_copy_from_ip4(peer.endpoint_ip, request->endpoint);
    peer.endport_port = config.endpointPort;
    peer.keep_alive = config.keepaliveSeconds;

    uint8_t index = WIREGUARDIF_INVALID_INDEX;
    if (wireguardif_add_peer(added, &peer, &index) != ERR_OK || index == WIREGUARDIF_INVALID_INDEX) {
        wireguardif_shutdown(added);
        netif_remove(added);
        request->error = "Invalid VPN peer";
        return ERR_ARG;
    }
    wireguardif_connect(added, index);

    tunnel->fullTunnel_ = config.allowedPrefix == 0;
    if (tunnel->fullTunnel_) {
        tunnel->previousDefault_ = netif_default;
        netif_set_default(added);
    }
    tunnel->peerIndex_ = index;
    tunnel->netif_ = added;
    return ERR_OK;
}

void WireGuardTunnel::stop() {
    if (netif_ == nullptr) return;
    TunnelCall request{};
    request.tunnel = this;
    tcpip_api_call(stopInNetworkThread, &request.call);
}

err_t WireGuardTunnel::stopInNetworkThread(struct tcpip_api_call_data* call) {
    WireGuardTunnel* tunnel = reinterpret_cast<TunnelCall*>(call)->tunnel;
    if (tunnel->netif_ == nullptr) return ERR_OK;
    if (tunnel->fullTunnel_) netif_set_default(tunnel->previousDefault_);
    wireguardif_disconnect(tunnel->netif_, tunnel->peerIndex_);
    wireguardif_remove_peer(tunnel->netif_, tunnel->peerIndex_);
    wireguardif_shutdown(tunnel->netif_);
    netif_remove(tunnel->netif_);
    tunnel->netif_ = nullptr;
    tunnel->previousDefault_ = nullptr;
    tunnel->peerIndex_ = WIREGUARDIF_INVALID_INDEX;
    tunnel->fullTunnel_ = false;
    return ERR_OK;
}

bool WireGuardTunnel::peerUp() {
    if (netif_ == nullptr) return false;
    TunnelCall request{};
    request.tunnel = this;
    tcpip_api_call(peerUpInNetworkThread, &request.call);
    return request.result;
}

err_t WireGuardTunnel::peerUpInNetworkThread(struct tcpip_api_call_data* call) {
    TunnelCall* request = reinterpret_cast<TunnelCall*>(call);
    WireGuardTunnel* tunnel = request->tunnel;
    ip_addr_t currentIp;
    u16_t currentPort = 0;
    request->result = tunnel->netif_ != nullptr &&
                      wireguardif_peer_is_up(tunnel->netif_, tunnel->peerIndex_, &currentIp, &currentPort) == ERR_OK;
    return ERR_OK;
}

bool WireGuardTunnel::generatePrivateKey(char (&privateKey)[kWireGuardKeyCapacity]) {
    uint8_t key[kWireGuardKeyBytes];
    esp_fill_random(key, sizeof(key));
    // Curve25519 clamping, as `wg genkey` does.
    key[0] &= 248;
    key[31] &= 127;
    key[31] |= 64;
    size_t length = sizeof(privateKey);
    const bool encoded = wireguard_base64_encode(key, sizeof(key), privateKey, &length);
    secureClear(key, sizeof(key));
    return encoded && isWireGuardKey(privateKey);
}

bool WireGuardTunnel::publicKeyFor(const char* privateKey, char (&publicKey)[kWireGuardKeyCapacity]) {
    if (!isWireGuardKey(privateKey)) return false;
    uint8_t secret[kWireGuardKeyBytes];
    size_t secretLength = sizeof(secret);
    if (!wireguard_base64_decode(privateKey, secret, &secretLength) || secretLength != sizeof(secret)) {
        secureClear(secret, sizeof(secret));
        return false;
    }
    uint8_t point[kWireGuardKeyBytes];
    x25519_base(point, secret, 1);
    secureClear(secret, sizeof(secret));
    size_t length = sizeof(publicKey);
    return wireguard_base64_encode(point, sizeof(point), publicKey, &length) && isWireGuardKey(publicKey);
}

bool clockLooksValid() {
    return time(nullptr) > kEarliestPlausibleTime;
}

bool syncClock(uint32_t timeoutMs, const OperationCallbacks& callbacks) {
    if (clockLooksValid()) return true;
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    const uint32_t startedAt = millis();
    while (!clockLooksValid()) {
        if (callbacks.isCanceled() || millis() - startedAt >= timeoutMs) return false;
        callbacks.report("Syncing clock", static_cast<uint16_t>((millis() - startedAt) / 1000U),
                         static_cast<uint16_t>(timeoutMs / 1000U));
        delay(100);
    }
    return true;
}

}  // namespace app
