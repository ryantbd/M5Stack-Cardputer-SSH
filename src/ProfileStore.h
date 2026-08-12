#pragma once

#include <stddef.h>
#include <stdint.h>

#include "AppConfig.h"

namespace app {

enum class BlobStatus : uint8_t { Ok, Missing, Error };

class ConfigBlobStorage {
public:
    virtual ~ConfigBlobStorage() = default;
    virtual BlobStatus length(const char* key, size_t& result) = 0;
    virtual bool read(const char* key, void* destination, size_t length) = 0;
    virtual bool write(const char* key, const void* source, size_t length) = 0;
    virtual bool remove(const char* key) = 0;
};

enum class ProfileLoadStatus : uint8_t {
    LoadedV2,
    MigratedV1,
    Missing,
    Invalid,
    StorageError,
};

class ProfileStore {
public:
    explicit ProfileStore(ConfigBlobStorage& storage) : storage_(storage) {}

    ProfileLoadStatus load(ProfileConfigData& config);
    bool save(const ProfileConfigData& config);
    bool clearAll();

private:
    bool writeAndVerify(const ProfileConfigData& config);

    ConfigBlobStorage& storage_;
};

}  // namespace app
