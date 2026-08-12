#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <cstring>

#include "ProfileStore.h"

void setUp() {}
void tearDown() {}

namespace {

constexpr char kLegacyKey[] = "config";
constexpr char kV2Key[] = "config-v2";

class FakeBlobStorage final : public app::ConfigBlobStorage {
public:
    app::BlobStatus length(const char* key, size_t& result) override {
        if (failLength) {
            return app::BlobStatus::Error;
        }
        if (std::strcmp(key, kLegacyKey) == 0) {
            result = legacyLength;
            return legacyLength == 0 ? app::BlobStatus::Missing : app::BlobStatus::Ok;
        }
        if (std::strcmp(key, kV2Key) == 0) {
            result = v2Length;
            return v2Length == 0 ? app::BlobStatus::Missing : app::BlobStatus::Ok;
        }
        result = 0;
        return app::BlobStatus::Missing;
    }

    bool read(const char* key, void* destination, size_t lengthValue) override {
        if (failRead || destination == nullptr) {
            return false;
        }
        if (std::strcmp(key, kLegacyKey) == 0 && lengthValue == legacyLength) {
            ++legacyReads;
            std::memcpy(destination, legacy, lengthValue);
            return true;
        }
        if (std::strcmp(key, kV2Key) == 0 && lengthValue == v2Length) {
            ++v2Reads;
            std::memcpy(destination, v2, lengthValue);
            if (corruptNextV2Read && lengthValue > 0) {
                static_cast<uint8_t*>(destination)[0] ^= 0x01;
                corruptNextV2Read = false;
            }
            return true;
        }
        return false;
    }

    bool write(const char* key, const void* source, size_t lengthValue) override {
        if (failWrite || source == nullptr || std::strcmp(key, kV2Key) != 0 || lengthValue != sizeof(v2)) {
            return false;
        }
        ++v2Writes;
        std::memcpy(v2, source, lengthValue);
        v2Length = lengthValue;
        return true;
    }

    bool remove(const char* key) override {
        if (failRemove) {
            return false;
        }
        if (std::strcmp(key, kLegacyKey) == 0) {
            std::memset(legacy, 0, sizeof(legacy));
            legacyLength = 0;
            return true;
        }
        if (std::strcmp(key, kV2Key) == 0) {
            std::memset(v2, 0, sizeof(v2));
            v2Length = 0;
            return true;
        }
        return true;
    }

    void setLegacy(const app::LegacyConfigV1& config) {
        std::memcpy(legacy, &config, sizeof(config));
        legacyLength = sizeof(config);
    }

    void setV2(const app::ProfileConfigData& config) {
        std::memcpy(v2, &config, sizeof(config));
        v2Length = sizeof(config);
    }

    uint8_t legacy[sizeof(app::LegacyConfigV1)]{};
    uint8_t v2[sizeof(app::ProfileConfigData)]{};
    size_t legacyLength = 0;
    size_t v2Length = 0;
    size_t legacyReads = 0;
    size_t v2Reads = 0;
    size_t v2Writes = 0;
    bool failLength = false;
    bool failRead = false;
    bool failWrite = false;
    bool failRemove = false;
    bool corruptNextV2Read = false;
};

app::LegacyConfigV1 validLegacy() {
    app::LegacyConfigV1 config;
    app::resetConfig(config);
    app::copyConfigString(config.wifiSsid, sizeof(config.wifiSsid), "unit-network");
    app::copyConfigString(config.sshHost, sizeof(config.sshHost), "192.0.2.42");
    app::copyConfigString(config.sshUser, sizeof(config.sshUser), "unit-user");
    app::copyConfigString(config.sshPassword, sizeof(config.sshPassword), "fixture-secret");
    app::finalizeConfig(config);
    return config;
}

app::ProfileConfigData validV2() {
    app::ProfileConfigData config;
    TEST_ASSERT_TRUE(app::migrateConfigV1(validLegacy(), config));
    return config;
}

void testMissingBothKeysReturnsMissing() {
    FakeBlobStorage backend;
    app::ProfileStore store(backend);
    app::ProfileConfigData output;
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::Missing, store.load(output));
}

void testValidV2WinsWithoutReadingLegacy() {
    FakeBlobStorage backend;
    backend.setLegacy(validLegacy());
    const app::ProfileConfigData expected = validV2();
    backend.setV2(expected);
    app::ProfileStore store(backend);
    app::ProfileConfigData output;
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::LoadedV2, store.load(output));
    TEST_ASSERT_EQUAL_MEMORY(&expected, &output, sizeof(output));
    TEST_ASSERT_EQUAL_UINT32(0, backend.legacyReads);
}

void testInvalidPresentV2NeverFallsBackToLegacy() {
    FakeBlobStorage backend;
    backend.setLegacy(validLegacy());
    app::ProfileConfigData invalid = validV2();
    invalid.crc32 ^= 1;
    backend.setV2(invalid);
    app::ProfileStore store(backend);
    app::ProfileConfigData output;
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::Invalid, store.load(output));
    TEST_ASSERT_EQUAL_UINT32(0, backend.legacyReads);
}

void testMigrationWritesVerifiedV2WithoutChangingLegacy() {
    FakeBlobStorage backend;
    const app::LegacyConfigV1 legacy = validLegacy();
    backend.setLegacy(legacy);
    uint8_t original[sizeof(backend.legacy)]{};
    std::memcpy(original, backend.legacy, sizeof(original));
    app::ProfileStore store(backend);
    app::ProfileConfigData output;
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::MigratedV1, store.load(output));
    TEST_ASSERT_TRUE(app::validateProfileConfig(output));
    TEST_ASSERT_EQUAL_UINT32(1, backend.v2Writes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original, backend.legacy, sizeof(original));
}

void testReadbackCorruptionLeavesLegacyUntouched() {
    FakeBlobStorage backend;
    backend.setLegacy(validLegacy());
    uint8_t original[sizeof(backend.legacy)]{};
    std::memcpy(original, backend.legacy, sizeof(original));
    backend.corruptNextV2Read = true;
    app::ProfileStore store(backend);
    app::ProfileConfigData output;
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::StorageError, store.load(output));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(original, backend.legacy, sizeof(original));
}

void testStorageFailuresRemainDistinct() {
    FakeBlobStorage backend;
    app::ProfileStore store(backend);
    app::ProfileConfigData output;
    backend.failLength = true;
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::StorageError, store.load(output));

    backend = FakeBlobStorage{};
    backend.setLegacy(validLegacy());
    backend.failRead = true;
    app::ProfileStore readFailStore(backend);
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::StorageError, readFailStore.load(output));

    backend = FakeBlobStorage{};
    backend.setLegacy(validLegacy());
    backend.failWrite = true;
    app::ProfileStore writeFailStore(backend);
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::StorageError, writeFailStore.load(output));
}

void testWrongBlobSizesAreInvalid() {
    FakeBlobStorage backend;
    backend.v2Length = sizeof(app::ProfileConfigData) - 1;
    app::ProfileStore store(backend);
    app::ProfileConfigData output;
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::Invalid, store.load(output));

    backend = FakeBlobStorage{};
    backend.legacyLength = sizeof(app::LegacyConfigV1) - 1;
    app::ProfileStore legacyStore(backend);
    TEST_ASSERT_EQUAL(app::ProfileLoadStatus::Invalid, legacyStore.load(output));
}

void testSaveRejectsInvalidAndVerifiesValidBlob() {
    FakeBlobStorage backend;
    app::ProfileStore store(backend);
    app::ProfileConfigData invalid = validV2();
    invalid.crc32 ^= 1;
    TEST_ASSERT_FALSE(store.save(invalid));
    TEST_ASSERT_EQUAL_UINT32(0, backend.v2Writes);

    const app::ProfileConfigData valid = validV2();
    TEST_ASSERT_TRUE(store.save(valid));
    TEST_ASSERT_EQUAL_UINT32(1, backend.v2Writes);
}

void testSaveRejectsCorruptReadbackWithoutChangingInput() {
    FakeBlobStorage backend;
    backend.corruptNextV2Read = true;
    app::ProfileStore store(backend);
    const app::ProfileConfigData input = validV2();
    const uint32_t originalCrc = input.crc32;

    TEST_ASSERT_FALSE(store.save(input));
    TEST_ASSERT_EQUAL_UINT32(1, backend.v2Writes);
    TEST_ASSERT_EQUAL_UINT32(originalCrc, input.crc32);
    TEST_ASSERT_TRUE(app::validateProfileConfig(input));
}

void testClearAllRemovesBothKeys() {
    FakeBlobStorage backend;
    backend.setLegacy(validLegacy());
    backend.setV2(validV2());
    app::ProfileStore store(backend);
    TEST_ASSERT_TRUE(store.clearAll());
    TEST_ASSERT_EQUAL_UINT32(0, backend.legacyLength);
    TEST_ASSERT_EQUAL_UINT32(0, backend.v2Length);
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testMissingBothKeysReturnsMissing);
    RUN_TEST(testValidV2WinsWithoutReadingLegacy);
    RUN_TEST(testInvalidPresentV2NeverFallsBackToLegacy);
    RUN_TEST(testMigrationWritesVerifiedV2WithoutChangingLegacy);
    RUN_TEST(testReadbackCorruptionLeavesLegacyUntouched);
    RUN_TEST(testStorageFailuresRemainDistinct);
    RUN_TEST(testWrongBlobSizesAreInvalid);
    RUN_TEST(testSaveRejectsInvalidAndVerifiesValidBlob);
    RUN_TEST(testSaveRejectsCorruptReadbackWithoutChangingInput);
    RUN_TEST(testClearAllRemovesBothKeys);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runTests();
}
void loop() {}
#else
int main(int, char**) {
    return runTests();
}
#endif
