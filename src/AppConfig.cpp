#include "AppConfig.h"

#include <string.h>

namespace app {

namespace {

uint32_t crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

bool hasNonZeroByte(const uint8_t* data, size_t length) {
    uint8_t combined = 0;
    for (size_t index = 0; index < length; ++index) {
        combined |= data[index];
    }
    return combined != 0;
}

bool allZero(const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint8_t combined = 0;
    for (size_t index = 0; index < length; ++index) {
        combined |= bytes[index];
    }
    return combined == 0;
}

bool validBool(uint8_t value) {
    return value <= 1;
}

bool validWifiProfile(const WifiProfile& profile) {
    if (!isConfigStringTerminated(profile.ssid, sizeof(profile.ssid)) ||
        !isConfigStringTerminated(profile.password, sizeof(profile.password)) ||
        profile.ssid[0] == '\0' ||
        profile.priority > static_cast<uint8_t>(WifiPriority::High) || !validBool(profile.hidden) ||
        !validBool(profile.enabled) || profile.reserved != 0) {
        return false;
    }
    return true;
}

bool validHostProfile(const SshHostProfile& profile) {
    if (!isConfigStringTerminated(profile.displayName, sizeof(profile.displayName)) ||
        !isConfigStringTerminated(profile.host, sizeof(profile.host)) ||
        !isConfigStringTerminated(profile.user, sizeof(profile.user)) ||
        !isConfigStringTerminated(profile.password, sizeof(profile.password)) || profile.displayName[0] == '\0' ||
        profile.host[0] == '\0' || profile.user[0] == '\0' || profile.port == 0 ||
        !validBool(profile.savePassword) || !validBool(profile.hostKeyTrusted) || !validBool(profile.enabled) ||
        !allZero(profile.reserved, sizeof(profile.reserved))) {
        return false;
    }

    if ((profile.savePassword != 0) != (profile.password[0] != '\0')) {
        return false;
    }

    const bool digestPresent = hasNonZeroByte(profile.hostKeySha256, sizeof(profile.hostKeySha256));
    return (profile.hostKeyTrusted != 0) == digestPresent;
}

bool duplicateWifiSsid(const ProfileConfigData& config, const char* ssid, size_t ignoredIndex) {
    for (size_t index = 0; index < config.wifiCount; ++index) {
        if (index != ignoredIndex && strcmp(config.wifi[index].ssid, ssid) == 0) {
            return true;
        }
    }
    return false;
}

bool commitProfileCandidate(ProfileConfigData& destination, ProfileConfigData& candidate) {
    finalizeProfileConfig(candidate);
    if (!validateProfileConfig(candidate)) {
        secureClear(&candidate, sizeof(candidate));
        return false;
    }
    destination = candidate;
    secureClear(&candidate, sizeof(candidate));
    return true;
}

bool replaceHostProfileImpl(ProfileConfigData& config,
                            size_t index,
                            const SshHostProfile& profile,
                            bool endpointAlreadyVerified) {
    if (!validateProfileConfig(config) || index >= config.hostCount || !validHostProfile(profile)) {
        return false;
    }
    ProfileConfigData candidate = config;
    SshHostProfile replacement = profile;
    if (!endpointAlreadyVerified &&
        (strcmp(candidate.hosts[index].host, replacement.host) != 0 ||
         candidate.hosts[index].port != replacement.port)) {
        replacement.hostKeyTrusted = 0;
        memset(replacement.hostKeySha256, 0, sizeof(replacement.hostKeySha256));
    }
    candidate.hosts[index] = replacement;
    secureClear(&replacement, sizeof(replacement));
    return commitProfileCandidate(config, candidate);
}

}  // namespace

void resetConfig(AppConfigData& config) {
    memset(&config, 0, sizeof(config));
    config.magic = kConfigMagic;
    config.version = kConfigVersion;
    config.structSize = static_cast<uint16_t>(sizeof(config));
    config.sshPort = kDefaultSshPort;
}

void finalizeConfig(AppConfigData& config) {
    config.magic = kConfigMagic;
    config.version = kConfigVersion;
    config.structSize = static_cast<uint16_t>(sizeof(config));
    memset(config.reserved, 0, sizeof(config.reserved));
    config.crc32 = calculateConfigCrc(config);
}

bool validateConfig(const AppConfigData& config) {
    if (config.magic != kConfigMagic || config.version != kConfigVersion ||
        config.structSize != sizeof(config) || config.sshPort == 0) {
        return false;
    }

    if (!isConfigStringTerminated(config.wifiSsid, sizeof(config.wifiSsid)) ||
        !isConfigStringTerminated(config.wifiPassword, sizeof(config.wifiPassword)) ||
        !isConfigStringTerminated(config.sshHost, sizeof(config.sshHost)) ||
        !isConfigStringTerminated(config.sshUser, sizeof(config.sshUser)) ||
        !isConfigStringTerminated(config.sshPassword, sizeof(config.sshPassword))) {
        return false;
    }

    if (config.wifiSsid[0] == '\0' || config.sshHost[0] == '\0' || config.sshUser[0] == '\0' ||
        config.sshPassword[0] == '\0' || config.hostKeyTrusted > 1) {
        return false;
    }

    if (config.hostKeyTrusted != 0 && !hasNonZeroByte(config.hostKeySha256, sizeof(config.hostKeySha256))) {
        return false;
    }

    return config.crc32 == calculateConfigCrc(config);
}

uint32_t calculateConfigCrc(const AppConfigData& config) {
    return crc32(reinterpret_cast<const uint8_t*>(&config), offsetof(AppConfigData, crc32));
}

void resetProfileConfig(ProfileConfigData& config) {
    memset(&config, 0, sizeof(config));
    finalizeProfileConfig(config);
}

void finalizeProfileConfig(ProfileConfigData& config) {
    config.magic = kConfigMagic;
    config.version = kProfileConfigVersion;
    config.structSize = static_cast<uint16_t>(sizeof(config));
    memset(config.reservedHeader, 0, sizeof(config.reservedHeader));
    for (size_t index = 0; index < kMaxWifiProfiles; ++index) {
        config.wifi[index].reserved = 0;
    }
    for (size_t index = 0; index < kMaxSshHostProfiles; ++index) {
        memset(config.hosts[index].reserved, 0, sizeof(config.hosts[index].reserved));
    }
    config.crc32 = calculateProfileConfigCrc(config);
}

bool validateProfileConfig(const ProfileConfigData& config) {
    if (config.magic != kConfigMagic || config.version != kProfileConfigVersion ||
        config.structSize != sizeof(config) || config.wifiCount > kMaxWifiProfiles ||
        config.hostCount > kMaxSshHostProfiles || !allZero(config.reservedHeader, sizeof(config.reservedHeader))) {
        return false;
    }

    for (size_t index = 0; index < config.wifiCount; ++index) {
        if (!validWifiProfile(config.wifi[index]) ||
            duplicateWifiSsid(config, config.wifi[index].ssid, index)) {
            return false;
        }
    }
    for (size_t index = config.wifiCount; index < kMaxWifiProfiles; ++index) {
        if (!allZero(&config.wifi[index], sizeof(config.wifi[index]))) {
            return false;
        }
    }

    for (size_t index = 0; index < config.hostCount; ++index) {
        if (!validHostProfile(config.hosts[index])) {
            return false;
        }
    }
    for (size_t index = config.hostCount; index < kMaxSshHostProfiles; ++index) {
        if (!allZero(&config.hosts[index], sizeof(config.hosts[index]))) {
            return false;
        }
    }

    return config.crc32 == calculateProfileConfigCrc(config);
}

uint32_t calculateProfileConfigCrc(const ProfileConfigData& config) {
    return crc32(reinterpret_cast<const uint8_t*>(&config), offsetof(ProfileConfigData, crc32));
}

bool addWifiProfile(ProfileConfigData& config, const WifiProfile& profile) {
    if (!validateProfileConfig(config) || config.wifiCount >= kMaxWifiProfiles || !validWifiProfile(profile) ||
        duplicateWifiSsid(config, profile.ssid, kMaxWifiProfiles)) {
        return false;
    }
    ProfileConfigData candidate = config;
    candidate.wifi[candidate.wifiCount] = profile;
    ++candidate.wifiCount;
    return commitProfileCandidate(config, candidate);
}

bool replaceWifiProfile(ProfileConfigData& config, size_t index, const WifiProfile& profile) {
    if (!validateProfileConfig(config) || index >= config.wifiCount || !validWifiProfile(profile) ||
        duplicateWifiSsid(config, profile.ssid, index)) {
        return false;
    }
    ProfileConfigData candidate = config;
    candidate.wifi[index] = profile;
    return commitProfileCandidate(config, candidate);
}

bool deleteWifiProfile(ProfileConfigData& config, size_t index) {
    if (!validateProfileConfig(config) || index >= config.wifiCount) {
        return false;
    }
    ProfileConfigData candidate = config;
    for (size_t move = index + 1; move < candidate.wifiCount; ++move) {
        candidate.wifi[move - 1] = candidate.wifi[move];
    }
    --candidate.wifiCount;
    secureClear(&candidate.wifi[candidate.wifiCount], sizeof(candidate.wifi[candidate.wifiCount]));
    return commitProfileCandidate(config, candidate);
}

bool addHostProfile(ProfileConfigData& config, const SshHostProfile& profile) {
    if (!validateProfileConfig(config) || config.hostCount >= kMaxSshHostProfiles || !validHostProfile(profile)) {
        return false;
    }
    ProfileConfigData candidate = config;
    candidate.hosts[candidate.hostCount] = profile;
    ++candidate.hostCount;
    return commitProfileCandidate(config, candidate);
}

bool replaceHostProfile(ProfileConfigData& config, size_t index, const SshHostProfile& profile) {
    return replaceHostProfileImpl(config, index, profile, false);
}

bool replaceVerifiedHostProfile(ProfileConfigData& config,
                                size_t index,
                                const SshHostProfile& profile) {
    return replaceHostProfileImpl(config, index, profile, true);
}

bool deleteHostProfile(ProfileConfigData& config, size_t index) {
    if (!validateProfileConfig(config) || index >= config.hostCount) {
        return false;
    }
    ProfileConfigData candidate = config;
    for (size_t move = index + 1; move < candidate.hostCount; ++move) {
        candidate.hosts[move - 1] = candidate.hosts[move];
    }
    --candidate.hostCount;
    secureClear(&candidate.hosts[candidate.hostCount], sizeof(candidate.hosts[candidate.hostCount]));
    return commitProfileCandidate(config, candidate);
}

bool moveHostProfile(ProfileConfigData& config, size_t from, size_t to) {
    if (!validateProfileConfig(config) || from >= config.hostCount || to >= config.hostCount || from == to) {
        return false;
    }
    ProfileConfigData candidate = config;
    SshHostProfile moved = candidate.hosts[from];
    if (from < to) {
        for (size_t index = from; index < to; ++index) {
            candidate.hosts[index] = candidate.hosts[index + 1];
        }
    } else {
        for (size_t index = from; index > to; --index) {
            candidate.hosts[index] = candidate.hosts[index - 1];
        }
    }
    candidate.hosts[to] = moved;
    secureClear(&moved, sizeof(moved));
    return commitProfileCandidate(config, candidate);
}

bool migrateConfigV1(const LegacyConfigV1& legacy, ProfileConfigData& destination) {
    secureClear(&destination, sizeof(destination));
    if (!validateConfig(legacy)) {
        return false;
    }

    ProfileConfigData candidate;
    resetProfileConfig(candidate);
    candidate.wifiCount = 1;
    candidate.wifi[0].priority = static_cast<uint8_t>(WifiPriority::Normal);
    candidate.wifi[0].enabled = 1;
    candidate.hostCount = 1;
    candidate.hosts[0].port = legacy.sshPort;
    candidate.hosts[0].savePassword = 1;
    candidate.hosts[0].hostKeyTrusted = legacy.hostKeyTrusted;
    candidate.hosts[0].enabled = 1;

    const bool copied = copyConfigString(candidate.wifi[0].ssid, sizeof(candidate.wifi[0].ssid), legacy.wifiSsid) &&
                        copyConfigString(candidate.wifi[0].password,
                                         sizeof(candidate.wifi[0].password),
                                         legacy.wifiPassword) &&
                        copyConfigString(candidate.hosts[0].displayName,
                                         sizeof(candidate.hosts[0].displayName),
                                         "Imported host") &&
                        copyConfigString(candidate.hosts[0].host,
                                         sizeof(candidate.hosts[0].host),
                                         legacy.sshHost) &&
                        copyConfigString(candidate.hosts[0].user,
                                         sizeof(candidate.hosts[0].user),
                                         legacy.sshUser) &&
                        copyConfigString(candidate.hosts[0].password,
                                         sizeof(candidate.hosts[0].password),
                                         legacy.sshPassword);
    if (!copied) {
        secureClear(&candidate, sizeof(candidate));
        return false;
    }

    if (legacy.hostKeyTrusted != 0) {
        memcpy(candidate.hosts[0].hostKeySha256, legacy.hostKeySha256, sizeof(legacy.hostKeySha256));
    }
    finalizeProfileConfig(candidate);
    if (!validateProfileConfig(candidate)) {
        secureClear(&candidate, sizeof(candidate));
        return false;
    }

    destination = candidate;
    secureClear(&candidate, sizeof(candidate));
    return true;
}

bool copyConfigString(char* destination, size_t capacity, const char* source) {
    if (destination == nullptr || source == nullptr || capacity == 0) {
        return false;
    }

    size_t length = 0;
    while (length < capacity && source[length] != '\0') {
        ++length;
    }
    if (length >= capacity) {
        return false;
    }

    memset(destination, 0, capacity);
    memcpy(destination, source, length);
    return true;
}

bool isConfigStringTerminated(const char* value, size_t capacity) {
    if (value == nullptr || capacity == 0) {
        return false;
    }
    for (size_t index = 0; index < capacity; ++index) {
        if (value[index] == '\0') {
            return true;
        }
    }
    return false;
}

void secureClear(void* data, size_t length) {
    volatile uint8_t* bytes = static_cast<volatile uint8_t*>(data);
    while (length-- > 0) {
        *bytes++ = 0;
    }
}

uint32_t calculateCrc32(const void* data, size_t length) {
    return crc32(static_cast<const uint8_t*>(data), length);
}

}  // namespace app
