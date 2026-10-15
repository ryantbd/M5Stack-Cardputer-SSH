#pragma once

#include <stddef.h>
#include <stdint.h>

namespace app {

constexpr uint32_t kConfigMagic = 0x5049434FU;  // "PICO"
constexpr uint16_t kConfigVersion = 1;
constexpr uint16_t kProfileConfigVersion = 2;
constexpr uint16_t kDefaultSshPort = 22;

constexpr size_t kWifiSsidCapacity = 33;
constexpr size_t kWifiPasswordCapacity = 64;
constexpr size_t kSshHostCapacity = 65;
constexpr size_t kSshUserCapacity = 33;
constexpr size_t kSshPasswordCapacity = 129;
constexpr size_t kHostKeyHashSize = 32;
constexpr size_t kHostDisplayNameCapacity = 33;
constexpr size_t kMaxWifiProfiles = 4;
constexpr size_t kMaxSshHostProfiles = 8;

enum class WifiPriority : uint8_t { Low = 0, Normal = 1, High = 2 };

#pragma pack(push, 1)
struct LegacyConfigV1 {
    uint32_t magic;
    uint16_t version;
    uint16_t structSize;
    char wifiSsid[kWifiSsidCapacity];
    char wifiPassword[kWifiPasswordCapacity];
    char sshHost[kSshHostCapacity];
    uint16_t sshPort;
    char sshUser[kSshUserCapacity];
    char sshPassword[kSshPasswordCapacity];
    uint8_t hostKeySha256[kHostKeyHashSize];
    uint8_t hostKeyTrusted;
    uint8_t reserved[7];
    uint32_t crc32;
};

using AppConfigData = LegacyConfigV1;

struct WifiProfile {
    char ssid[kWifiSsidCapacity];
    char password[kWifiPasswordCapacity];
    uint8_t priority;
    uint8_t hidden;
    uint8_t enabled;
    uint8_t reserved;
};

struct SshHostProfile {
    char displayName[kHostDisplayNameCapacity];
    char host[kSshHostCapacity];
    uint16_t port;
    char user[kSshUserCapacity];
    char password[kSshPasswordCapacity];
    uint8_t hostKeySha256[kHostKeyHashSize];
    uint8_t savePassword;
    uint8_t hostKeyTrusted;
    uint8_t enabled;
    uint8_t reserved[3];
};

struct ProfileConfigData {
    uint32_t magic;
    uint16_t version;
    uint16_t structSize;
    uint8_t wifiCount;
    uint8_t hostCount;
    uint8_t reservedHeader[4];
    WifiProfile wifi[kMaxWifiProfiles];
    SshHostProfile hosts[kMaxSshHostProfiles];
    uint32_t crc32;
};
#pragma pack(pop)

static_assert(sizeof(LegacyConfigV1) == 378, "Legacy V1 layout must remain byte-compatible");
static_assert(sizeof(WifiProfile) == 101, "Wi-Fi profile layout changed");
static_assert(sizeof(SshHostProfile) == 300, "SSH host profile layout changed");
static_assert(sizeof(ProfileConfigData) == 2822, "Profile config layout changed");
static_assert(kMaxWifiProfiles == 4, "V2 Wi-Fi slot count changed");
static_assert(kMaxSshHostProfiles == 8, "V2 SSH host slot count changed");
static_assert(kHostKeyHashSize == 32, "Host key digest must remain SHA-256 sized");

void resetConfig(AppConfigData& config);
void finalizeConfig(AppConfigData& config);
bool validateConfig(const AppConfigData& config);
uint32_t calculateConfigCrc(const AppConfigData& config);

void resetProfileConfig(ProfileConfigData& config);
void finalizeProfileConfig(ProfileConfigData& config);
bool validateProfileConfig(const ProfileConfigData& config);
uint32_t calculateProfileConfigCrc(const ProfileConfigData& config);

bool addWifiProfile(ProfileConfigData& config, const WifiProfile& profile);
bool replaceWifiProfile(ProfileConfigData& config, size_t index, const WifiProfile& profile);
bool deleteWifiProfile(ProfileConfigData& config, size_t index);

bool addHostProfile(ProfileConfigData& config, const SshHostProfile& profile);
bool replaceHostProfile(ProfileConfigData& config, size_t index, const SshHostProfile& profile);
bool replaceVerifiedHostProfile(ProfileConfigData& config, size_t index, const SshHostProfile& profile);
bool deleteHostProfile(ProfileConfigData& config, size_t index);
bool moveHostProfile(ProfileConfigData& config, size_t from, size_t to);

bool migrateConfigV1(const LegacyConfigV1& legacy, ProfileConfigData& destination);

bool copyConfigString(char* destination, size_t capacity, const char* source);
bool isConfigStringTerminated(const char* value, size_t capacity);
void secureClear(void* data, size_t length);
uint32_t calculateCrc32(const void* data, size_t length);

}  // namespace app
