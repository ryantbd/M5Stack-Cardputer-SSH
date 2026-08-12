#include "ProfileStore.h"

#include <new>
#include <string.h>

namespace app {

namespace {

constexpr char kLegacyKey[] = "config";
constexpr char kV2Key[] = "config-v2";

ProfileLoadStatus blobLength(ConfigBlobStorage& storage,
                             const char* key,
                             size_t expected,
                             bool& present) {
    size_t length = 0;
    const BlobStatus status = storage.length(key, length);
    present = status == BlobStatus::Ok;
    if (status == BlobStatus::Error) {
        return ProfileLoadStatus::StorageError;
    }
    if (status == BlobStatus::Missing) {
        return ProfileLoadStatus::Missing;
    }
    return length == expected ? ProfileLoadStatus::LoadedV2 : ProfileLoadStatus::Invalid;
}

}  // namespace

ProfileLoadStatus ProfileStore::load(ProfileConfigData& config) {
    secureClear(&config, sizeof(config));

    bool v2Present = false;
    const ProfileLoadStatus v2LengthStatus = blobLength(storage_, kV2Key, sizeof(ProfileConfigData), v2Present);
    if (v2Present) {
        if (v2LengthStatus != ProfileLoadStatus::LoadedV2) {
            return v2LengthStatus;
        }
        ProfileConfigData candidate{};
        if (!storage_.read(kV2Key, &candidate, sizeof(candidate))) {
            secureClear(&candidate, sizeof(candidate));
            return ProfileLoadStatus::StorageError;
        }
        if (!validateProfileConfig(candidate)) {
            secureClear(&candidate, sizeof(candidate));
            return ProfileLoadStatus::Invalid;
        }
        config = candidate;
        secureClear(&candidate, sizeof(candidate));
        return ProfileLoadStatus::LoadedV2;
    }
    if (v2LengthStatus == ProfileLoadStatus::StorageError) {
        return v2LengthStatus;
    }

    bool legacyPresent = false;
    const ProfileLoadStatus legacyLengthStatus =
        blobLength(storage_, kLegacyKey, sizeof(LegacyConfigV1), legacyPresent);
    if (!legacyPresent) {
        return legacyLengthStatus;
    }
    if (legacyLengthStatus == ProfileLoadStatus::Invalid ||
        legacyLengthStatus == ProfileLoadStatus::StorageError) {
        return legacyLengthStatus;
    }

    LegacyConfigV1 legacy{};
    if (!storage_.read(kLegacyKey, &legacy, sizeof(legacy))) {
        secureClear(&legacy, sizeof(legacy));
        return ProfileLoadStatus::StorageError;
    }
    if (!validateConfig(legacy)) {
        secureClear(&legacy, sizeof(legacy));
        return ProfileLoadStatus::Invalid;
    }

    ProfileConfigData migrated{};
    const bool migratedOk = migrateConfigV1(legacy, migrated);
    secureClear(&legacy, sizeof(legacy));
    if (!migratedOk) {
        secureClear(&migrated, sizeof(migrated));
        return ProfileLoadStatus::Invalid;
    }
    if (!writeAndVerify(migrated)) {
        secureClear(&migrated, sizeof(migrated));
        return ProfileLoadStatus::StorageError;
    }

    config = migrated;
    secureClear(&migrated, sizeof(migrated));
    return ProfileLoadStatus::MigratedV1;
}

bool ProfileStore::save(const ProfileConfigData& config) {
    return validateProfileConfig(config) && writeAndVerify(config);
}

bool ProfileStore::clearAll() {
    const bool v2Removed = storage_.remove(kV2Key);
    const bool legacyRemoved = storage_.remove(kLegacyKey);
    return v2Removed && legacyRemoved;
}

bool ProfileStore::writeAndVerify(const ProfileConfigData& config) {
    if (!validateProfileConfig(config) || !storage_.write(kV2Key, &config, sizeof(config))) {
        return false;
    }

    size_t storedLength = 0;
    if (storage_.length(kV2Key, storedLength) != BlobStatus::Ok || storedLength != sizeof(config)) {
        return false;
    }

    ProfileConfigData* readBack = new (std::nothrow) ProfileConfigData{};
    if (readBack == nullptr) {
        return false;
    }

    bool valid = storage_.read(kV2Key, readBack, sizeof(*readBack));
    if (valid) {
        valid = validateProfileConfig(*readBack) && memcmp(&config, readBack, sizeof(config)) == 0;
    }
    secureClear(readBack, sizeof(*readBack));
    delete readBack;
    return valid;
}

}  // namespace app
