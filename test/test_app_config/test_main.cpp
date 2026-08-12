#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <cstring>
#include <cstdio>

#include "AppConfig.h"

void setUp() {}
void tearDown() {}

namespace {

app::AppConfigData validConfig() {
    app::AppConfigData config;
    app::resetConfig(config);
    TEST_ASSERT_TRUE(app::copyConfigString(config.wifiSsid, sizeof(config.wifiSsid), "unit-network"));
    TEST_ASSERT_TRUE(app::copyConfigString(config.wifiPassword, sizeof(config.wifiPassword), ""));
    TEST_ASSERT_TRUE(app::copyConfigString(config.sshHost, sizeof(config.sshHost), "192.0.2.42"));
    TEST_ASSERT_TRUE(app::copyConfigString(config.sshUser, sizeof(config.sshUser), "unit-user"));
    TEST_ASSERT_TRUE(app::copyConfigString(config.sshPassword, sizeof(config.sshPassword), "test-only"));
    for (size_t index = 0; index < sizeof(config.hostKeySha256); ++index) {
        config.hostKeySha256[index] = static_cast<uint8_t>(index + 1U);
    }
    config.hostKeyTrusted = 1;
    app::finalizeConfig(config);
    return config;
}

app::ProfileConfigData validProfileConfig() {
    app::ProfileConfigData config;
    app::resetProfileConfig(config);
    config.wifiCount = 1;
    TEST_ASSERT_TRUE(app::copyConfigString(config.wifi[0].ssid,
                                           sizeof(config.wifi[0].ssid),
                                           "unit-network"));
    config.wifi[0].priority = static_cast<uint8_t>(app::WifiPriority::Normal);
    config.wifi[0].enabled = 1;

    config.hostCount = 1;
    TEST_ASSERT_TRUE(app::copyConfigString(config.hosts[0].displayName,
                                           sizeof(config.hosts[0].displayName),
                                           "unit-host"));
    TEST_ASSERT_TRUE(app::copyConfigString(config.hosts[0].host,
                                           sizeof(config.hosts[0].host),
                                           "192.0.2.42"));
    TEST_ASSERT_TRUE(app::copyConfigString(config.hosts[0].user,
                                           sizeof(config.hosts[0].user),
                                           "unit-user"));
    config.hosts[0].port = app::kDefaultSshPort;
    config.hosts[0].enabled = 1;
    app::finalizeProfileConfig(config);
    return config;
}

app::LegacyConfigV1 trustedLegacyConfig() {
    app::LegacyConfigV1 config = validConfig();
    for (size_t index = 0; index < sizeof(config.hostKeySha256); ++index) {
        config.hostKeySha256[index] = static_cast<uint8_t>(0x80U + index);
    }
    config.hostKeyTrusted = 1;
    app::finalizeConfig(config);
    return config;
}

void testResetUsesCurrentFormatAndDefaultPort() {
    app::AppConfigData config;
    std::memset(&config, 0xA5, sizeof(config));
    app::resetConfig(config);

    TEST_ASSERT_EQUAL_UINT32(app::kConfigMagic, config.magic);
    TEST_ASSERT_EQUAL_UINT16(app::kConfigVersion, config.version);
    TEST_ASSERT_EQUAL_UINT16(sizeof(config), config.structSize);
    TEST_ASSERT_EQUAL_UINT16(app::kDefaultSshPort, config.sshPort);
    TEST_ASSERT_EQUAL_CHAR('\0', config.wifiSsid[0]);
    TEST_ASSERT_FALSE(app::validateConfig(config));
}

void testFinalizedConfigurationValidates() {
    const app::AppConfigData config = validConfig();
    TEST_ASSERT_TRUE(app::validateConfig(config));
    TEST_ASSERT_EQUAL_UINT32(config.crc32, app::calculateConfigCrc(config));
}

void testCrcRejectsSingleByteCorruption() {
    app::AppConfigData config = validConfig();
    config.sshHost[0] ^= 0x01;
    TEST_ASSERT_FALSE(app::validateConfig(config));
}

void testUnknownVersionAndWrongSizeAreRejected() {
    app::AppConfigData config = validConfig();
    ++config.version;
    app::finalizeConfig(config);
    ++config.version;
    TEST_ASSERT_FALSE(app::validateConfig(config));

    config = validConfig();
    --config.structSize;
    config.crc32 = app::calculateConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateConfig(config));
}

void testRequiredAndUnterminatedFieldsAreRejected() {
    app::AppConfigData config = validConfig();
    config.sshUser[0] = '\0';
    config.crc32 = app::calculateConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateConfig(config));

    config = validConfig();
    std::memset(config.wifiSsid, 'x', sizeof(config.wifiSsid));
    config.crc32 = app::calculateConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateConfig(config));
}

void testCopyStringEnforcesCapacityAndTermination() {
    char destination[5]{};
    TEST_ASSERT_TRUE(app::copyConfigString(destination, sizeof(destination), "four"));
    TEST_ASSERT_EQUAL_STRING("four", destination);

    TEST_ASSERT_FALSE(app::copyConfigString(destination, sizeof(destination), "five5"));
    TEST_ASSERT_EQUAL_STRING("four", destination);
    TEST_ASSERT_FALSE(app::copyConfigString(nullptr, sizeof(destination), "x"));
    TEST_ASSERT_FALSE(app::copyConfigString(destination, 0, "x"));
}

void testTrustFlagAndDigestAreValidated() {
    app::AppConfigData config = validConfig();
    config.hostKeyTrusted = 2;
    config.crc32 = app::calculateConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateConfig(config));

    config = validConfig();
    std::memset(config.hostKeySha256, 0, sizeof(config.hostKeySha256));
    config.crc32 = app::calculateConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateConfig(config));
}

void testLegacyLayoutRemainsExact() {
    TEST_ASSERT_EQUAL_UINT32(378, sizeof(app::LegacyConfigV1));
    TEST_ASSERT_EQUAL_UINT32(sizeof(app::LegacyConfigV1), sizeof(app::AppConfigData));
}

void testProfileResetUsesVersionTwoAndFixedCounts() {
    app::ProfileConfigData config;
    std::memset(&config, 0xA5, sizeof(config));
    app::resetProfileConfig(config);

    TEST_ASSERT_EQUAL_UINT32(app::kConfigMagic, config.magic);
    TEST_ASSERT_EQUAL_UINT16(app::kProfileConfigVersion, config.version);
    TEST_ASSERT_EQUAL_UINT16(sizeof(config), config.structSize);
    TEST_ASSERT_EQUAL_UINT8(0, config.wifiCount);
    TEST_ASSERT_EQUAL_UINT8(0, config.hostCount);
}

void testFinalizedProfileConfigurationValidates() {
    const app::ProfileConfigData config = validProfileConfig();
    TEST_ASSERT_TRUE(app::validateProfileConfig(config));
    TEST_ASSERT_EQUAL_UINT32(config.crc32, app::calculateProfileConfigCrc(config));
}

void testProfileValidationRejectsStructuralCorruption() {
    app::ProfileConfigData config = validProfileConfig();
    config.wifiCount = static_cast<uint8_t>(app::kMaxWifiProfiles + 1U);
    config.crc32 = app::calculateProfileConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateProfileConfig(config));

    config = validProfileConfig();
    config.reservedHeader[0] = 1;
    config.crc32 = app::calculateProfileConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateProfileConfig(config));

    config = validProfileConfig();
    config.wifi[1].enabled = 1;
    config.crc32 = app::calculateProfileConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateProfileConfig(config));
}

void testProfileValidationRejectsPasswordAndTrustMismatch() {
    app::ProfileConfigData config = validProfileConfig();
    TEST_ASSERT_TRUE(app::copyConfigString(config.hosts[0].password,
                                           sizeof(config.hosts[0].password),
                                           "fixture-secret"));
    config.crc32 = app::calculateProfileConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateProfileConfig(config));

    config = validProfileConfig();
    config.hosts[0].savePassword = 1;
    config.crc32 = app::calculateProfileConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateProfileConfig(config));

    config = validProfileConfig();
    config.hosts[0].hostKeySha256[0] = 1;
    config.crc32 = app::calculateProfileConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateProfileConfig(config));
}

void testProfileValidationRejectsDuplicateSsid() {
    app::ProfileConfigData config = validProfileConfig();
    config.wifi[1] = config.wifi[0];
    config.wifiCount = 2;
    config.crc32 = app::calculateProfileConfigCrc(config);
    TEST_ASSERT_FALSE(app::validateProfileConfig(config));
}

void testWifiOperationsEnforceCapacityAndCompactDeletion() {
    app::ProfileConfigData config = validProfileConfig();
    for (size_t index = 1; index < app::kMaxWifiProfiles; ++index) {
        app::WifiProfile profile{};
        char ssid[16]{};
        std::snprintf(ssid, sizeof(ssid), "unit-%u", static_cast<unsigned>(index));
        TEST_ASSERT_TRUE(app::copyConfigString(profile.ssid, sizeof(profile.ssid), ssid));
        profile.priority = static_cast<uint8_t>(app::WifiPriority::Normal);
        profile.enabled = 1;
        TEST_ASSERT_TRUE(app::addWifiProfile(config, profile));
    }
    TEST_ASSERT_EQUAL_UINT8(app::kMaxWifiProfiles, config.wifiCount);
    TEST_ASSERT_FALSE(app::addWifiProfile(config, config.wifi[0]));

    const char expected[16] = "unit-3";
    TEST_ASSERT_TRUE(app::deleteWifiProfile(config, 2));
    TEST_ASSERT_EQUAL_UINT8(app::kMaxWifiProfiles - 1U, config.wifiCount);
    TEST_ASSERT_EQUAL_STRING(expected, config.wifi[2].ssid);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, &config.wifi[config.wifiCount], sizeof(app::WifiProfile));
    TEST_ASSERT_TRUE(app::validateProfileConfig(config));
}

void testHostTargetChangeClearsTrustButCredentialChangePreservesIt() {
    app::ProfileConfigData config = validProfileConfig();
    config.hosts[0].hostKeyTrusted = 1;
    std::memset(config.hosts[0].hostKeySha256, 0x5A, app::kHostKeyHashSize);
    app::finalizeProfileConfig(config);

    app::SshHostProfile replacement = config.hosts[0];
    TEST_ASSERT_TRUE(app::copyConfigString(replacement.user,
                                           sizeof(replacement.user),
                                           "other-user"));
    TEST_ASSERT_TRUE(app::replaceHostProfile(config, 0, replacement));
    TEST_ASSERT_EQUAL_UINT8(1, config.hosts[0].hostKeyTrusted);

    replacement = config.hosts[0];
    TEST_ASSERT_TRUE(app::copyConfigString(replacement.host,
                                           sizeof(replacement.host),
                                           "198.51.100.9"));
    TEST_ASSERT_TRUE(app::replaceHostProfile(config, 0, replacement));
    TEST_ASSERT_EQUAL_UINT8(0, config.hosts[0].hostKeyTrusted);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, config.hosts[0].hostKeySha256, app::kHostKeyHashSize);
}

void testVerifiedHostReplacementPreservesAuthenticatedTrust() {
    app::ProfileConfigData config = validProfileConfig();
    app::SshHostProfile replacement = config.hosts[0];
    TEST_ASSERT_TRUE(app::copyConfigString(replacement.host,
                                           sizeof(replacement.host),
                                           "198.51.100.10"));
    replacement.hostKeyTrusted = 1;
    std::memset(replacement.hostKeySha256, 0xA5, app::kHostKeyHashSize);

    TEST_ASSERT_TRUE(app::replaceVerifiedHostProfile(config, 0, replacement));
    TEST_ASSERT_EQUAL_UINT8(1, config.hosts[0].hostKeyTrusted);
    TEST_ASSERT_EACH_EQUAL_UINT8(0xA5, config.hosts[0].hostKeySha256, app::kHostKeyHashSize);
    TEST_ASSERT_TRUE(app::validateProfileConfig(config));
}

void testHostOperationsReorderAndCompact() {
    app::ProfileConfigData config = validProfileConfig();
    app::SshHostProfile second = config.hosts[0];
    TEST_ASSERT_TRUE(app::copyConfigString(second.displayName,
                                           sizeof(second.displayName),
                                           "second"));
    TEST_ASSERT_TRUE(app::copyConfigString(second.host,
                                           sizeof(second.host),
                                           "192.0.2.43"));
    TEST_ASSERT_TRUE(app::addHostProfile(config, second));
    TEST_ASSERT_TRUE(app::moveHostProfile(config, 1, 0));
    TEST_ASSERT_EQUAL_STRING("second", config.hosts[0].displayName);
    TEST_ASSERT_TRUE(app::deleteHostProfile(config, 0));
    TEST_ASSERT_EQUAL_UINT8(1, config.hostCount);
    TEST_ASSERT_EQUAL_STRING("unit-host", config.hosts[0].displayName);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, &config.hosts[1], sizeof(app::SshHostProfile));
}

void testMigrationPreservesTrustedLegacyFields() {
    const app::LegacyConfigV1 legacy = trustedLegacyConfig();
    app::ProfileConfigData migrated;
    TEST_ASSERT_TRUE(app::migrateConfigV1(legacy, migrated));
    TEST_ASSERT_EQUAL_UINT8(1, migrated.wifiCount);
    TEST_ASSERT_EQUAL_UINT8(1, migrated.hostCount);
    TEST_ASSERT_EQUAL_STRING(legacy.wifiSsid, migrated.wifi[0].ssid);
    TEST_ASSERT_EQUAL_STRING("Imported host", migrated.hosts[0].displayName);
    TEST_ASSERT_EQUAL_STRING(legacy.sshHost, migrated.hosts[0].host);
    TEST_ASSERT_EQUAL_STRING(legacy.sshUser, migrated.hosts[0].user);
    TEST_ASSERT_EQUAL_STRING(legacy.sshPassword, migrated.hosts[0].password);
    TEST_ASSERT_EQUAL_UINT16(legacy.sshPort, migrated.hosts[0].port);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(legacy.hostKeySha256,
                                 migrated.hosts[0].hostKeySha256,
                                 app::kHostKeyHashSize);
    TEST_ASSERT_TRUE(app::validateProfileConfig(migrated));
}

void testMigrationClearsStaleUntrustedDigest() {
    app::LegacyConfigV1 legacy = validConfig();
    std::memset(legacy.hostKeySha256, 0xA5, sizeof(legacy.hostKeySha256));
    legacy.hostKeyTrusted = 0;
    app::finalizeConfig(legacy);
    TEST_ASSERT_TRUE(app::validateConfig(legacy));

    app::ProfileConfigData migrated;
    TEST_ASSERT_TRUE(app::migrateConfigV1(legacy, migrated));
    TEST_ASSERT_EQUAL_UINT8(0, migrated.hosts[0].hostKeyTrusted);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, migrated.hosts[0].hostKeySha256, app::kHostKeyHashSize);
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testResetUsesCurrentFormatAndDefaultPort);
    RUN_TEST(testFinalizedConfigurationValidates);
    RUN_TEST(testCrcRejectsSingleByteCorruption);
    RUN_TEST(testUnknownVersionAndWrongSizeAreRejected);
    RUN_TEST(testRequiredAndUnterminatedFieldsAreRejected);
    RUN_TEST(testCopyStringEnforcesCapacityAndTermination);
    RUN_TEST(testTrustFlagAndDigestAreValidated);
    RUN_TEST(testLegacyLayoutRemainsExact);
    RUN_TEST(testProfileResetUsesVersionTwoAndFixedCounts);
    RUN_TEST(testFinalizedProfileConfigurationValidates);
    RUN_TEST(testProfileValidationRejectsStructuralCorruption);
    RUN_TEST(testProfileValidationRejectsPasswordAndTrustMismatch);
    RUN_TEST(testProfileValidationRejectsDuplicateSsid);
    RUN_TEST(testWifiOperationsEnforceCapacityAndCompactDeletion);
    RUN_TEST(testHostTargetChangeClearsTrustButCredentialChangePreservesIt);
    RUN_TEST(testVerifiedHostReplacementPreservesAuthenticatedTrust);
    RUN_TEST(testHostOperationsReorderAndCompact);
    RUN_TEST(testMigrationPreservesTrustedLegacyFields);
    RUN_TEST(testMigrationClearsStaleUntrustedDigest);
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
