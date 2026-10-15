#include "DeviceIdentity.h"

#include <string.h>

#include "AppConfig.h"

namespace app {

namespace {

constexpr char kOpenSshPrivateKeyHeader[] = "-----BEGIN OPENSSH PRIVATE KEY-----";

}  // namespace

void resetIdentity(DeviceIdentityData& identity) {
    secureClear(&identity, sizeof(identity));
    identity.magic = kIdentityMagic;
    identity.version = kIdentityVersion;
    identity.structSize = static_cast<uint16_t>(sizeof(identity));
}

void finalizeIdentity(DeviceIdentityData& identity) {
    identity.magic = kIdentityMagic;
    identity.version = kIdentityVersion;
    identity.structSize = static_cast<uint16_t>(sizeof(identity));
    memset(identity.reserved, 0, sizeof(identity.reserved));
    identity.crc32 = calculateCrc32(&identity, offsetof(DeviceIdentityData, crc32));
}

bool validateIdentity(const DeviceIdentityData& identity) {
    if (identity.magic != kIdentityMagic || identity.version != kIdentityVersion ||
        identity.structSize != sizeof(identity) ||
        !isConfigStringTerminated(identity.sshPrivateKey, sizeof(identity.sshPrivateKey))) {
        return false;
    }
    for (uint8_t value : identity.reserved) {
        if (value != 0) return false;
    }
    if (identity.sshPrivateKey[0] != '\0' &&
        strncmp(identity.sshPrivateKey, kOpenSshPrivateKeyHeader, strlen(kOpenSshPrivateKeyHeader)) != 0) {
        return false;
    }
    return identity.crc32 == calculateCrc32(&identity, offsetof(DeviceIdentityData, crc32));
}

bool identityHasSshKey(const DeviceIdentityData& identity) {
    return identity.sshPrivateKey[0] != '\0';
}

}  // namespace app
