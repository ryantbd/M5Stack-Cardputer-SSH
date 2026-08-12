#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <cstdio>
#include <cstring>
#include "HostCatalog.h"

void setUp() {}
void tearDown() {}

app::SshHostProfile makeHost(unsigned index) {
    app::SshHostProfile host{};
    std::snprintf(host.displayName, sizeof(host.displayName), "host-%u", index);
    std::snprintf(host.host, sizeof(host.host), "192.0.2.%u", index + 1);
    app::copyConfigString(host.user, sizeof(host.user), "user");
    host.port = 22; host.enabled = 1;
    return host;
}

void testCatalogFiltersAndMovesEnabledHosts() {
    app::ProfileConfigData config; app::resetProfileConfig(config);
    for (unsigned i = 0; i < 3; ++i) TEST_ASSERT_TRUE(app::HostCatalog::add(config, makeHost(i)));
    TEST_ASSERT_TRUE(app::HostCatalog::setEnabled(config, 1, false));
    uint8_t indices[8]{};
    TEST_ASSERT_EQUAL_UINT32(2, app::HostCatalog::launcherIndices(config, indices, 8));
    TEST_ASSERT_EQUAL_UINT8(0, indices[0]); TEST_ASSERT_EQUAL_UINT8(2, indices[1]);
    TEST_ASSERT_TRUE(app::HostCatalog::moveUp(config, 2));
    TEST_ASSERT_EQUAL_STRING("host-2", config.hosts[1].displayName);
    TEST_ASSERT_TRUE(app::validateProfileConfig(config));
}

void testResetTrustClearsDigest() {
    app::ProfileConfigData config; app::resetProfileConfig(config);
    auto host = makeHost(0); host.hostKeyTrusted = 1; std::memset(host.hostKeySha256, 0x5a, 32);
    TEST_ASSERT_TRUE(app::HostCatalog::add(config, host));
    TEST_ASSERT_TRUE(app::HostCatalog::resetTrust(config, 0));
    TEST_ASSERT_EQUAL_UINT8(0, config.hosts[0].hostKeyTrusted);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, config.hosts[0].hostKeySha256, 32);
}

void testVerifiedReplacementKeepsNewEndpointDigest() {
    app::ProfileConfigData config; app::resetProfileConfig(config);
    TEST_ASSERT_TRUE(app::HostCatalog::add(config, makeHost(0)));
    auto replacement = config.hosts[0];
    app::copyConfigString(replacement.host, sizeof(replacement.host), "198.51.100.20");
    replacement.hostKeyTrusted = 1;
    std::memset(replacement.hostKeySha256, 0xA6, app::kHostKeyHashSize);
    TEST_ASSERT_TRUE(app::HostCatalog::replaceVerified(config, 0, replacement));
    TEST_ASSERT_EQUAL_UINT8(1, config.hosts[0].hostKeyTrusted);
    TEST_ASSERT_EACH_EQUAL_UINT8(0xA6, config.hosts[0].hostKeySha256, app::kHostKeyHashSize);
}

int runTests() { UNITY_BEGIN(); RUN_TEST(testCatalogFiltersAndMovesEnabledHosts); RUN_TEST(testResetTrustClearsDigest); RUN_TEST(testVerifiedReplacementKeepsNewEndpointDigest); return UNITY_END(); }
#ifdef ARDUINO
void setup(){delay(2000);runTests();} void loop(){}
#else
int main(int,char**){return runTests();}
#endif
