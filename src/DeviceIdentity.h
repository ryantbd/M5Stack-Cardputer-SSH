#pragma once

#include <stddef.h>
#include <stdint.h>

namespace app {

constexpr uint32_t kIdentityMagic = 0x4449534BU;  // "KSID"
constexpr uint16_t kIdentityVersion = 1;
// An unencrypted OpenSSH-format Ed25519 private key is about 400 characters.
constexpr size_t kSshPrivateKeyCapacity = 640;

#pragma pack(push, 1)
// The device's own SSH key. It is generated on the device and never exported;
// only its public half is shown, for adding to a server's authorized_keys.
struct DeviceIdentityData {
    uint32_t magic;
    uint16_t version;
    uint16_t structSize;
    char sshPrivateKey[kSshPrivateKeyCapacity];
    uint8_t reserved[4];
    uint32_t crc32;
};
#pragma pack(pop)

static_assert(sizeof(DeviceIdentityData) == 656, "Identity layout changed");

void resetIdentity(DeviceIdentityData& identity);
void finalizeIdentity(DeviceIdentityData& identity);
bool validateIdentity(const DeviceIdentityData& identity);
bool identityHasSshKey(const DeviceIdentityData& identity);

}  // namespace app
