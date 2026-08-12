#pragma once

#include "ProfileStore.h"

namespace app {

class NvsConfigStorage final : public ConfigBlobStorage {
public:
    BlobStatus length(const char* key, size_t& result) override;
    bool read(const char* key, void* destination, size_t length) override;
    bool write(const char* key, const void* source, size_t length) override;
    bool remove(const char* key) override;
};

}  // namespace app
