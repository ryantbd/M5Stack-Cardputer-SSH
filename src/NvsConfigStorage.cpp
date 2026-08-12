#include "NvsConfigStorage.h"

#include <Preferences.h>

namespace app {

namespace {

constexpr char kNamespace[] = "pi-console";

}  // namespace

BlobStatus NvsConfigStorage::length(const char* key, size_t& result) {
    result = 0;
    if (key == nullptr || key[0] == '\0') {
        return BlobStatus::Error;
    }

    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) {
        return BlobStatus::Error;
    }
    if (!preferences.isKey(key)) {
        preferences.end();
        return BlobStatus::Missing;
    }
    result = preferences.getBytesLength(key);
    preferences.end();
    return result == 0 ? BlobStatus::Error : BlobStatus::Ok;
}

bool NvsConfigStorage::read(const char* key, void* destination, size_t lengthValue) {
    if (key == nullptr || key[0] == '\0' || destination == nullptr || lengthValue == 0) {
        return false;
    }
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) {
        return false;
    }
    const size_t readLength = preferences.getBytes(key, destination, lengthValue);
    preferences.end();
    return readLength == lengthValue;
}

bool NvsConfigStorage::write(const char* key, const void* source, size_t lengthValue) {
    if (key == nullptr || key[0] == '\0' || source == nullptr || lengthValue == 0) {
        return false;
    }
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return false;
    }
    const size_t written = preferences.putBytes(key, source, lengthValue);
    preferences.end();
    return written == lengthValue;
}

bool NvsConfigStorage::remove(const char* key) {
    if (key == nullptr || key[0] == '\0') {
        return false;
    }
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return false;
    }
    if (!preferences.isKey(key)) {
        preferences.end();
        return true;
    }
    const bool removed = preferences.remove(key);
    preferences.end();
    return removed;
}

}  // namespace app
