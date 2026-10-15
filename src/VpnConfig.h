#pragma once

#include <stddef.h>
#include <stdint.h>

namespace app {

constexpr uint32_t kVpnConfigMagic = 0x4E50564BU;  // "KVPN"
constexpr uint16_t kVpnConfigVersion = 1;
constexpr size_t kWireGuardKeyCapacity = 45;  // 44 base64 characters + NUL
constexpr size_t kVpnEndpointCapacity = 65;
constexpr uint16_t kDefaultWireGuardPort = 51820;
constexpr uint16_t kDefaultKeepaliveSeconds = 25;
constexpr size_t kMaxWgQuickConfigLength = 2048;

#pragma pack(push, 1)
// One WireGuard peer, IPv4 only. Traffic for allowedIp/allowedPrefix goes
// through the tunnel; everything else keeps using Wi-Fi directly.
struct VpnConfigData {
    uint32_t magic;
    uint16_t version;
    uint16_t structSize;
    uint8_t enabled;
    uint8_t allowedPrefix;
    uint16_t endpointPort;
    uint16_t keepaliveSeconds;
    uint8_t address[4];
    uint8_t allowedIp[4];
    char privateKey[kWireGuardKeyCapacity];
    char peerPublicKey[kWireGuardKeyCapacity];
    char presharedKey[kWireGuardKeyCapacity];
    char endpointHost[kVpnEndpointCapacity];
    uint8_t reserved[4];
    uint32_t crc32;
};
#pragma pack(pop)

static_assert(sizeof(VpnConfigData) == 230, "VPN config layout changed");

enum class WgQuickParseStatus : uint8_t {
    Ok,
    Empty,
    TooLong,
    UnknownSection,
    MalformedLine,
    MissingPeer,
    MultiplePeers,
    InvalidKey,
    InvalidAddress,
    InvalidAllowedIps,
    InvalidEndpoint,
    InvalidKeepalive,
};

void resetVpnConfig(VpnConfigData& config);
void finalizeVpnConfig(VpnConfigData& config);
// A stored config may be incomplete (no peer yet); it is only usable when
// vpnConfigReady() is also true.
bool validateVpnConfig(const VpnConfigData& config);
bool vpnConfigReady(const VpnConfigData& config);

bool isWireGuardKey(const char* value);

// Parses wg-quick text (the format `wg-quick` and the WireGuard apps export).
// Fields present in the text replace those in `config`; a missing PrivateKey
// keeps the existing one, so a device-generated key survives an import.
WgQuickParseStatus parseWgQuickConfig(const char* text, VpnConfigData& config);
const char* wgQuickParseStatusText(WgQuickParseStatus status);

bool parseIpv4(const char* text, uint8_t (&address)[4]);
bool parseIpv4Cidr(const char* text, uint8_t (&address)[4], uint8_t& prefix);
void formatIpv4(const uint8_t (&address)[4], char* destination, size_t capacity);

}  // namespace app
