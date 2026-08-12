#pragma once

#include "AppConfig.h"

namespace app {

class HostCatalog {
public:
    static size_t launcherIndices(const ProfileConfigData& config, uint8_t* output, size_t capacity);
    static bool add(ProfileConfigData& config, const SshHostProfile& host);
    static bool replace(ProfileConfigData& config, size_t index, const SshHostProfile& host);
    static bool replaceVerified(ProfileConfigData& config, size_t index, const SshHostProfile& host);
    static bool remove(ProfileConfigData& config, size_t index);
    static bool moveUp(ProfileConfigData& config, size_t index);
    static bool moveDown(ProfileConfigData& config, size_t index);
    static bool setEnabled(ProfileConfigData& config, size_t index, bool enabled);
    static bool resetTrust(ProfileConfigData& config, size_t index);
};

}  // namespace app
