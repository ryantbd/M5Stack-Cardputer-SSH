#include "VpnConfig.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AppConfig.h"

namespace app {

namespace {

enum class Section : uint8_t { None, Interface, Peer };

bool allZero(const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint8_t combined = 0;
    for (size_t index = 0; index < length; ++index) {
        combined |= bytes[index];
    }
    return combined == 0;
}

bool isBase64Char(char value) {
    return isalnum(static_cast<unsigned char>(value)) || value == '+' || value == '/';
}

// Index of a base64 character in the standard alphabet.
int base64Value(char value) {
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '+') return 62;
    if (value == '/') return 63;
    return -1;
}

char* trim(char* text) {
    while (*text != '\0' && isspace(static_cast<unsigned char>(*text))) ++text;
    char* end = text + strlen(text);
    while (end > text && isspace(static_cast<unsigned char>(end[-1]))) --end;
    *end = '\0';
    return text;
}

bool equalsIgnoreCase(const char* left, const char* right) {
    while (*left != '\0' && *right != '\0') {
        if (tolower(static_cast<unsigned char>(*left)) != tolower(static_cast<unsigned char>(*right))) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == *right;
}

bool parseUnsigned(const char* text, uint32_t maximum, uint32_t& result) {
    if (text == nullptr || *text == '\0') return false;
    uint32_t value = 0;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        if (!isdigit(static_cast<unsigned char>(*cursor))) return false;
        value = value * 10U + static_cast<uint32_t>(*cursor - '0');
        if (value > maximum) return false;
    }
    result = value;
    return true;
}

// Picks the first IPv4 entry of a comma-separated wg-quick list
// ("10.0.0.2/24, fd00::2/64"). IPv6 entries are skipped.
bool firstIpv4Cidr(char* list, uint8_t (&address)[4], uint8_t& prefix, bool defaultHostPrefix) {
    char* saveptr = nullptr;
    for (char* item = strtok_r(list, ",", &saveptr); item != nullptr; item = strtok_r(nullptr, ",", &saveptr)) {
        item = trim(item);
        if (strchr(item, ':') != nullptr) continue;
        if (strchr(item, '/') == nullptr && defaultHostPrefix) {
            if (!parseIpv4(item, address)) return false;
            prefix = 32;
            return true;
        }
        return parseIpv4Cidr(item, address, prefix);
    }
    return false;
}

bool parseEndpoint(const char* text, char* host, size_t hostCapacity, uint16_t& port) {
    if (text == nullptr || text[0] == '[') return false;  // IPv6 endpoints are not supported
    const char* colon = strrchr(text, ':');
    if (colon == nullptr || colon == text) return false;
    const size_t hostLength = static_cast<size_t>(colon - text);
    if (hostLength >= hostCapacity || memchr(text, ':', hostLength) != nullptr) return false;
    uint32_t portValue = 0;
    if (!parseUnsigned(colon + 1, 65535U, portValue) || portValue == 0) return false;
    memcpy(host, text, hostLength);
    host[hostLength] = '\0';
    port = static_cast<uint16_t>(portValue);
    return true;
}

}  // namespace

void resetVpnConfig(VpnConfigData& config) {
    memset(&config, 0, sizeof(config));
    config.magic = kVpnConfigMagic;
    config.version = kVpnConfigVersion;
    config.structSize = static_cast<uint16_t>(sizeof(config));
    config.endpointPort = kDefaultWireGuardPort;
    config.keepaliveSeconds = kDefaultKeepaliveSeconds;
}

void finalizeVpnConfig(VpnConfigData& config) {
    config.magic = kVpnConfigMagic;
    config.version = kVpnConfigVersion;
    config.structSize = static_cast<uint16_t>(sizeof(config));
    memset(config.reserved, 0, sizeof(config.reserved));
    config.crc32 = calculateCrc32(&config, offsetof(VpnConfigData, crc32));
}

bool validateVpnConfig(const VpnConfigData& config) {
    if (config.magic != kVpnConfigMagic || config.version != kVpnConfigVersion ||
        config.structSize != sizeof(config) || config.enabled > 1 || config.allowedPrefix > 32 ||
        !allZero(config.reserved, sizeof(config.reserved))) {
        return false;
    }
    if (!isConfigStringTerminated(config.privateKey, sizeof(config.privateKey)) ||
        !isConfigStringTerminated(config.peerPublicKey, sizeof(config.peerPublicKey)) ||
        !isConfigStringTerminated(config.presharedKey, sizeof(config.presharedKey)) ||
        !isConfigStringTerminated(config.endpointHost, sizeof(config.endpointHost))) {
        return false;
    }
    if ((config.privateKey[0] != '\0' && !isWireGuardKey(config.privateKey)) ||
        (config.peerPublicKey[0] != '\0' && !isWireGuardKey(config.peerPublicKey)) ||
        (config.presharedKey[0] != '\0' && !isWireGuardKey(config.presharedKey))) {
        return false;
    }
    return config.crc32 == calculateCrc32(&config, offsetof(VpnConfigData, crc32));
}

bool vpnConfigReady(const VpnConfigData& config) {
    return isWireGuardKey(config.privateKey) && isWireGuardKey(config.peerPublicKey) &&
           config.endpointHost[0] != '\0' && config.endpointPort != 0 &&
           !allZero(config.address, sizeof(config.address));
}

bool isWireGuardKey(const char* value) {
    // 32 bytes encode to 43 base64 characters plus one '=' of padding; the
    // last character carries two unused bits that must be zero.
    if (value == nullptr || strlen(value) != 44 || value[43] != '=') return false;
    for (size_t index = 0; index < 43; ++index) {
        if (!isBase64Char(value[index])) return false;
    }
    return (base64Value(value[42]) & 0x3) == 0;
}

WgQuickParseStatus parseWgQuickConfig(const char* text, VpnConfigData& config) {
    if (text == nullptr) return WgQuickParseStatus::Empty;
    const size_t length = strlen(text);
    if (length == 0) return WgQuickParseStatus::Empty;
    if (length > kMaxWgQuickConfigLength) return WgQuickParseStatus::TooLong;

    char buffer[kMaxWgQuickConfigLength + 1];
    memcpy(buffer, text, length + 1);

    VpnConfigData candidate = config;
    Section section = Section::None;
    size_t peerCount = 0;
    bool sawPeerKey = false;
    bool sawEndpoint = false;
    bool sawAllowedIps = false;
    WgQuickParseStatus status = WgQuickParseStatus::Ok;

    char* saveptr = nullptr;
    for (char* line = strtok_r(buffer, "\n", &saveptr); line != nullptr && status == WgQuickParseStatus::Ok;
         line = strtok_r(nullptr, "\n", &saveptr)) {
        char* comment = strchr(line, '#');
        if (comment != nullptr) *comment = '\0';
        line = trim(line);
        if (*line == '\0') continue;

        if (*line == '[') {
            if (equalsIgnoreCase(line, "[Interface]")) {
                section = Section::Interface;
            } else if (equalsIgnoreCase(line, "[Peer]")) {
                section = Section::Peer;
                if (++peerCount > 1) status = WgQuickParseStatus::MultiplePeers;
            } else {
                status = WgQuickParseStatus::UnknownSection;
            }
            continue;
        }

        char* equals = strchr(line, '=');
        if (equals == nullptr || section == Section::None) {
            status = WgQuickParseStatus::MalformedLine;
            continue;
        }
        *equals = '\0';
        const char* key = trim(line);
        char* value = trim(equals + 1);

        if (section == Section::Interface) {
            if (equalsIgnoreCase(key, "PrivateKey")) {
                if (!isWireGuardKey(value)) status = WgQuickParseStatus::InvalidKey;
                else copyConfigString(candidate.privateKey, sizeof(candidate.privateKey), value);
            } else if (equalsIgnoreCase(key, "Address")) {
                uint8_t prefix = 0;
                if (!firstIpv4Cidr(value, candidate.address, prefix, true))
                    status = WgQuickParseStatus::InvalidAddress;
            }
            // ListenPort, DNS, MTU, Table and the Pre/Post hooks don't apply here.
        } else {
            if (equalsIgnoreCase(key, "PublicKey")) {
                if (!isWireGuardKey(value)) status = WgQuickParseStatus::InvalidKey;
                else copyConfigString(candidate.peerPublicKey, sizeof(candidate.peerPublicKey), value);
                sawPeerKey = true;
            } else if (equalsIgnoreCase(key, "PresharedKey")) {
                if (!isWireGuardKey(value)) status = WgQuickParseStatus::InvalidKey;
                else copyConfigString(candidate.presharedKey, sizeof(candidate.presharedKey), value);
            } else if (equalsIgnoreCase(key, "Endpoint")) {
                if (!parseEndpoint(value, candidate.endpointHost, sizeof(candidate.endpointHost),
                                   candidate.endpointPort))
                    status = WgQuickParseStatus::InvalidEndpoint;
                sawEndpoint = true;
            } else if (equalsIgnoreCase(key, "AllowedIPs")) {
                if (!firstIpv4Cidr(value, candidate.allowedIp, candidate.allowedPrefix, false))
                    status = WgQuickParseStatus::InvalidAllowedIps;
                sawAllowedIps = true;
            } else if (equalsIgnoreCase(key, "PersistentKeepalive")) {
                uint32_t seconds = 0;
                if (equalsIgnoreCase(value, "off")) candidate.keepaliveSeconds = 0;
                else if (parseUnsigned(value, 65535U, seconds)) candidate.keepaliveSeconds = static_cast<uint16_t>(seconds);
                else status = WgQuickParseStatus::InvalidKeepalive;
            }
        }
    }

    if (status == WgQuickParseStatus::Ok) {
        if (peerCount == 0) status = WgQuickParseStatus::MissingPeer;
        else if (!sawPeerKey) status = WgQuickParseStatus::InvalidKey;
        else if (!sawEndpoint) status = WgQuickParseStatus::InvalidEndpoint;
        else if (!sawAllowedIps) status = WgQuickParseStatus::InvalidAllowedIps;
        else if (allZero(candidate.address, sizeof(candidate.address))) status = WgQuickParseStatus::InvalidAddress;
    }

    if (status == WgQuickParseStatus::Ok) {
        finalizeVpnConfig(candidate);
        config = candidate;
    }
    secureClear(&candidate, sizeof(candidate));
    secureClear(buffer, sizeof(buffer));
    return status;
}

const char* wgQuickParseStatusText(WgQuickParseStatus status) {
    switch (status) {
        case WgQuickParseStatus::Ok: return "OK";
        case WgQuickParseStatus::Empty: return "Config is empty";
        case WgQuickParseStatus::TooLong: return "Config is too long";
        case WgQuickParseStatus::UnknownSection: return "Unknown [section]";
        case WgQuickParseStatus::MalformedLine: return "Line is not Key = Value";
        case WgQuickParseStatus::MissingPeer: return "No [Peer] section";
        case WgQuickParseStatus::MultiplePeers: return "Only one [Peer] is supported";
        case WgQuickParseStatus::InvalidKey: return "Missing or invalid key";
        case WgQuickParseStatus::InvalidAddress: return "Missing or invalid IPv4 Address";
        case WgQuickParseStatus::InvalidAllowedIps: return "Missing or invalid IPv4 AllowedIPs";
        case WgQuickParseStatus::InvalidEndpoint: return "Endpoint must be host:port";
        case WgQuickParseStatus::InvalidKeepalive: return "Invalid PersistentKeepalive";
    }
    return "Unknown error";
}

bool parseIpv4(const char* text, uint8_t (&address)[4]) {
    if (text == nullptr) return false;
    uint8_t parsed[4]{};
    const char* cursor = text;
    for (size_t part = 0; part < 4; ++part) {
        if (!isdigit(static_cast<unsigned char>(*cursor))) return false;
        uint32_t value = 0;
        size_t digits = 0;
        while (isdigit(static_cast<unsigned char>(*cursor))) {
            value = value * 10U + static_cast<uint32_t>(*cursor - '0');
            if (++digits > 3 || value > 255U) return false;
            ++cursor;
        }
        parsed[part] = static_cast<uint8_t>(value);
        if (part < 3) {
            if (*cursor != '.') return false;
            ++cursor;
        }
    }
    if (*cursor != '\0') return false;
    memcpy(address, parsed, sizeof(parsed));
    return true;
}

bool parseIpv4Cidr(const char* text, uint8_t (&address)[4], uint8_t& prefix) {
    if (text == nullptr) return false;
    const char* slash = strchr(text, '/');
    if (slash == nullptr || static_cast<size_t>(slash - text) >= 16) return false;
    char ip[16]{};
    memcpy(ip, text, static_cast<size_t>(slash - text));
    uint32_t prefixValue = 0;
    uint8_t parsed[4]{};
    if (!parseIpv4(ip, parsed) || !parseUnsigned(slash + 1, 32U, prefixValue)) return false;
    memcpy(address, parsed, sizeof(parsed));
    prefix = static_cast<uint8_t>(prefixValue);
    return true;
}

void formatIpv4(const uint8_t (&address)[4], char* destination, size_t capacity) {
    if (destination == nullptr || capacity == 0) return;
    snprintf(destination, capacity, "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
}

}  // namespace app
