#include "HostCatalog.h"

#include <string.h>

namespace app {

size_t HostCatalog::launcherIndices(const ProfileConfigData& config, uint8_t* output, size_t capacity) {
    if (output == nullptr || capacity == 0 || !validateProfileConfig(config)) return 0;
    size_t count = 0;
    for (uint8_t index = 0; index < config.hostCount && count < capacity; ++index) {
        if (config.hosts[index].enabled != 0) output[count++] = index;
    }
    return count;
}

bool HostCatalog::add(ProfileConfigData& config, const SshHostProfile& host) {
    return addHostProfile(config, host);
}

bool HostCatalog::replace(ProfileConfigData& config, size_t index, const SshHostProfile& host) {
    return replaceHostProfile(config, index, host);
}

bool HostCatalog::replaceVerified(ProfileConfigData& config, size_t index, const SshHostProfile& host) {
    return replaceVerifiedHostProfile(config, index, host);
}

bool HostCatalog::remove(ProfileConfigData& config, size_t index) {
    return deleteHostProfile(config, index);
}

bool HostCatalog::moveUp(ProfileConfigData& config, size_t index) {
    return index > 0 && moveHostProfile(config, index, index - 1);
}

bool HostCatalog::moveDown(ProfileConfigData& config, size_t index) {
    return index + 1 < config.hostCount && moveHostProfile(config, index, index + 1);
}

bool HostCatalog::setEnabled(ProfileConfigData& config, size_t index, bool enabled) {
    if (index >= config.hostCount) return false;
    SshHostProfile host = config.hosts[index];
    host.enabled = enabled ? 1 : 0;
    return replaceHostProfile(config, index, host);
}

bool HostCatalog::resetTrust(ProfileConfigData& config, size_t index) {
    if (index >= config.hostCount) return false;
    SshHostProfile host = config.hosts[index];
    host.hostKeyTrusted = 0;
    memset(host.hostKeySha256, 0, sizeof(host.hostKeySha256));
    return replaceHostProfile(config, index, host);
}

}  // namespace app
