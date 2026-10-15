#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <cstring>

#include "AppConfig.h"
#include "DeviceIdentity.h"
#include "VpnConfig.h"

void setUp() {}
void tearDown() {}

namespace {

constexpr char kPrivateKey[] = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=";
constexpr char kPeerKey[] = "q6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6s=";
constexpr char kPresharedKey[] = "ERERERERERERERERERERERERERERERERERERERERERE=";

constexpr char kFullConfig[] =
    "[Interface]\n"
    "PrivateKey = AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=\n"
    "Address = 10.0.0.2/32\n"
    "DNS = 10.0.0.1\n"
    "\n"
    "[Peer]\n"
    "# hub\n"
    "PublicKey = q6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6s=\n"
    "Endpoint = vpn.example.net:443\n"
    "AllowedIPs = 10.0.0.0/24\n"
    "PersistentKeepalive = 25\n";

app::VpnConfigData freshConfig() {
    app::VpnConfigData config;
    app::resetVpnConfig(config);
    return config;
}

void assertAddress(const uint8_t (&actual)[4], uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    TEST_ASSERT_EQUAL_UINT8(a, actual[0]);
    TEST_ASSERT_EQUAL_UINT8(b, actual[1]);
    TEST_ASSERT_EQUAL_UINT8(c, actual[2]);
    TEST_ASSERT_EQUAL_UINT8(d, actual[3]);
}

void testParsesFullWgQuickConfig() {
    app::VpnConfigData config = freshConfig();
    TEST_ASSERT_EQUAL(app::WgQuickParseStatus::Ok, app::parseWgQuickConfig(kFullConfig, config));
    TEST_ASSERT_EQUAL_STRING(kPrivateKey, config.privateKey);
    TEST_ASSERT_EQUAL_STRING(kPeerKey, config.peerPublicKey);
    TEST_ASSERT_EQUAL_STRING("", config.presharedKey);
    TEST_ASSERT_EQUAL_STRING("vpn.example.net", config.endpointHost);
    TEST_ASSERT_EQUAL_UINT16(443, config.endpointPort);
    assertAddress(config.address, 10, 0, 0, 2);
    assertAddress(config.allowedIp, 10, 0, 0, 0);
    TEST_ASSERT_EQUAL_UINT8(24, config.allowedPrefix);
    TEST_ASSERT_EQUAL_UINT16(25, config.keepaliveSeconds);
    TEST_ASSERT_TRUE(app::validateVpnConfig(config));
    TEST_ASSERT_TRUE(app::vpnConfigReady(config));
}

void testImportWithoutPrivateKeyKeepsDeviceKey() {
    app::VpnConfigData config = freshConfig();
    TEST_ASSERT_TRUE(app::copyConfigString(config.privateKey, sizeof(config.privateKey), kPrivateKey));
    const char text[] =
        "[Interface]\r\n"
        "Address = 10.0.0.2\r\n"
        "[Peer]\r\n"
        "publickey=q6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6s=\r\n"
        "PresharedKey = ERERERERERERERERERERERERERERERERERERERERERE=\r\n"
        "Endpoint = 192.0.2.10:51820\r\n"
        "AllowedIPs = fd00::/64, 10.0.0.0/24\r\n";
    TEST_ASSERT_EQUAL(app::WgQuickParseStatus::Ok, app::parseWgQuickConfig(text, config));
    TEST_ASSERT_EQUAL_STRING(kPrivateKey, config.privateKey);
    TEST_ASSERT_EQUAL_STRING(kPresharedKey, config.presharedKey);
    TEST_ASSERT_EQUAL_STRING("192.0.2.10", config.endpointHost);
    assertAddress(config.allowedIp, 10, 0, 0, 0);
    TEST_ASSERT_EQUAL_UINT16(app::kDefaultKeepaliveSeconds, config.keepaliveSeconds);
}

void testRejectsBadConfigsWithoutChangingTarget() {
    struct Case {
        const char* text;
        app::WgQuickParseStatus expected;
    };
    const Case cases[] = {
        {"", app::WgQuickParseStatus::Empty},
        {"[Interface]\nAddress = 10.0.0.2\n", app::WgQuickParseStatus::MissingPeer},
        {"[Wat]\n", app::WgQuickParseStatus::UnknownSection},
        {"Address = 10.0.0.2\n", app::WgQuickParseStatus::MalformedLine},
        {"[Interface]\nAddress 10.0.0.2\n", app::WgQuickParseStatus::MalformedLine},
        {"[Interface]\nAddress = 10.0.0.2\n[Peer]\nPublicKey = q6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6s=\n"
         "Endpoint = h:1\nAllowedIPs = 10.0.0.0/24\n[Peer]\n",
         app::WgQuickParseStatus::MultiplePeers},
        {"[Interface]\nAddress = 10.0.0.2\n[Peer]\nPublicKey = short=\n", app::WgQuickParseStatus::InvalidKey},
        {"[Interface]\nAddress = 10.0.0.2\n[Peer]\nEndpoint = h:1\nAllowedIPs = 10.0.0.0/24\n",
         app::WgQuickParseStatus::InvalidKey},
        {"[Interface]\nAddress = 300.1.1.1\n", app::WgQuickParseStatus::InvalidAddress},
        {"[Interface]\nAddress = 10.0.0.2\n[Peer]\nPublicKey = q6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6s=\n"
         "Endpoint = [2001:db8::1]:51820\n",
         app::WgQuickParseStatus::InvalidEndpoint},
        {"[Interface]\nAddress = 10.0.0.2\n[Peer]\nPublicKey = q6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6s=\n"
         "Endpoint = host:0\n",
         app::WgQuickParseStatus::InvalidEndpoint},
        {"[Interface]\nAddress = 10.0.0.2\n[Peer]\nPublicKey = q6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6s=\n"
         "Endpoint = host:51820\nAllowedIPs = fd00::/64\n",
         app::WgQuickParseStatus::InvalidAllowedIps},
        {"[Interface]\nAddress = 10.0.0.2\n[Peer]\nPublicKey = q6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6s=\n"
         "Endpoint = host:51820\nAllowedIPs = 10.0.0.0/24\nPersistentKeepalive = soon\n",
         app::WgQuickParseStatus::InvalidKeepalive},
        {"[Peer]\nPublicKey = q6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6s=\nEndpoint = host:51820\n"
         "AllowedIPs = 10.0.0.0/24\n",
         app::WgQuickParseStatus::InvalidAddress},
    };
    for (const Case& testCase : cases) {
        app::VpnConfigData config = freshConfig();
        const app::VpnConfigData before = config;
        TEST_ASSERT_EQUAL_MESSAGE(testCase.expected, app::parseWgQuickConfig(testCase.text, config), testCase.text);
        TEST_ASSERT_EQUAL_MEMORY(&before, &config, sizeof(config));
    }
}

void testRejectsOversizedConfig() {
    static char text[app::kMaxWgQuickConfigLength + 2];
    memset(text, '#', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    app::VpnConfigData config = freshConfig();
    TEST_ASSERT_EQUAL(app::WgQuickParseStatus::TooLong, app::parseWgQuickConfig(text, config));
}

void testKeyFormatChecks() {
    TEST_ASSERT_TRUE(app::isWireGuardKey(kPrivateKey));
    TEST_ASSERT_FALSE(app::isWireGuardKey(nullptr));
    TEST_ASSERT_FALSE(app::isWireGuardKey(""));
    // Right length, but the final character sets padding bits.
    TEST_ASSERT_FALSE(app::isWireGuardKey("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh9="));
    TEST_ASSERT_FALSE(app::isWireGuardKey("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8"));
    TEST_ASSERT_FALSE(app::isWireGuardKey("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd!h8="));
}

void testStoredConfigValidation() {
    app::VpnConfigData config = freshConfig();
    TEST_ASSERT_EQUAL(app::WgQuickParseStatus::Ok, app::parseWgQuickConfig(kFullConfig, config));
    TEST_ASSERT_TRUE(app::validateVpnConfig(config));

    app::VpnConfigData tampered = config;
    tampered.endpointPort = 1;
    TEST_ASSERT_FALSE(app::validateVpnConfig(tampered));

    app::VpnConfigData empty = freshConfig();
    app::finalizeVpnConfig(empty);
    TEST_ASSERT_TRUE(app::validateVpnConfig(empty));
    TEST_ASSERT_FALSE(app::vpnConfigReady(empty));
}

void testIpv4Helpers() {
    uint8_t address[4]{};
    uint8_t prefix = 0;
    TEST_ASSERT_TRUE(app::parseIpv4("192.0.2.255", address));
    assertAddress(address, 192, 0, 2, 255);
    TEST_ASSERT_FALSE(app::parseIpv4("192.0.2", address));
    TEST_ASSERT_FALSE(app::parseIpv4("192.0.2.1.5", address));
    TEST_ASSERT_FALSE(app::parseIpv4("1922.0.2.1", address));
    TEST_ASSERT_TRUE(app::parseIpv4Cidr("0.0.0.0/0", address, prefix));
    TEST_ASSERT_EQUAL_UINT8(0, prefix);
    TEST_ASSERT_FALSE(app::parseIpv4Cidr("10.0.0.0/33", address, prefix));
    char text[16]{};
    const uint8_t value[4] = {10, 0, 0, 2};
    app::formatIpv4(value, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("10.0.0.2", text);
}

void testIdentityValidation() {
    app::DeviceIdentityData identity;
    app::resetIdentity(identity);
    app::finalizeIdentity(identity);
    TEST_ASSERT_TRUE(app::validateIdentity(identity));
    TEST_ASSERT_FALSE(app::identityHasSshKey(identity));

    TEST_ASSERT_TRUE(app::copyConfigString(identity.sshPrivateKey, sizeof(identity.sshPrivateKey),
                                           "-----BEGIN OPENSSH PRIVATE KEY-----\nAAAA\n"
                                           "-----END OPENSSH PRIVATE KEY-----\n"));
    app::finalizeIdentity(identity);
    TEST_ASSERT_TRUE(app::validateIdentity(identity));
    TEST_ASSERT_TRUE(app::identityHasSshKey(identity));

    app::DeviceIdentityData wrongFormat = identity;
    TEST_ASSERT_TRUE(app::copyConfigString(wrongFormat.sshPrivateKey, sizeof(wrongFormat.sshPrivateKey),
                                           "-----BEGIN RSA PRIVATE KEY-----\n"));
    app::finalizeIdentity(wrongFormat);
    TEST_ASSERT_FALSE(app::validateIdentity(wrongFormat));

    app::DeviceIdentityData corrupted = identity;
    corrupted.sshPrivateKey[40] ^= 0x01;
    TEST_ASSERT_FALSE(app::validateIdentity(corrupted));
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testParsesFullWgQuickConfig);
    RUN_TEST(testImportWithoutPrivateKeyKeepsDeviceKey);
    RUN_TEST(testRejectsBadConfigsWithoutChangingTarget);
    RUN_TEST(testRejectsOversizedConfig);
    RUN_TEST(testKeyFormatChecks);
    RUN_TEST(testStoredConfigValidation);
    RUN_TEST(testIpv4Helpers);
    RUN_TEST(testIdentityValidation);
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
