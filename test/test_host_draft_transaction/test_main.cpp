#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <string.h>

#include "HostDraftTransaction.h"

void setUp() {}
void tearDown() {}

namespace {

app::SshHostProfile fixtureProfile() {
    app::SshHostProfile profile{};
    app::copyConfigString(profile.displayName, sizeof(profile.displayName), "Draft host");
    app::copyConfigString(profile.host, sizeof(profile.host), "192.0.2.10");
    app::copyConfigString(profile.user, sizeof(profile.user), "unit-user");
    app::copyConfigString(profile.password, sizeof(profile.password), "fixture-secret");
    profile.port = 22;
    profile.enabled = 1;
    profile.savePassword = 1;
    return profile;
}

void assertStage(app::HostDraftStage expected, const app::HostDraftTransaction& draft) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>(draft.stage()));
}

void testPersistenceGateOpensOnlyAfterAuthentication() {
    const app::SshHostProfile profile = fixtureProfile();
    app::HostDraftTransaction draft(profile, -1);
    TEST_ASSERT_FALSE(draft.canPersist());
    TEST_ASSERT_TRUE(draft.beginAttempt());
    assertStage(app::HostDraftStage::Connecting, draft);
    TEST_ASSERT_TRUE(draft.authenticationSucceeded());
    assertStage(app::HostDraftStage::Authenticated, draft);
    TEST_ASSERT_TRUE(draft.canPersist());
    TEST_ASSERT_TRUE(draft.persistenceSucceeded());
    assertStage(app::HostDraftStage::Persisted, draft);
    TEST_ASSERT_FALSE(draft.canPersist());
}

void testFailedPersistenceReturnsToRecovery() {
    app::HostDraftTransaction draft(fixtureProfile(), 2);
    TEST_ASSERT_EQUAL_INT(2, draft.savedIndex());
    TEST_ASSERT_TRUE(draft.beginAttempt());
    TEST_ASSERT_TRUE(draft.authenticationSucceeded());
    TEST_ASSERT_TRUE(draft.persistenceFailed());
    assertStage(app::HostDraftStage::Recovery, draft);
    TEST_ASSERT_FALSE(draft.canPersist());
}

void testRetryAndEditTransitionsAreBounded() {
    app::HostDraftTransaction draft(fixtureProfile(), -1);
    TEST_ASSERT_TRUE(draft.beginAttempt());
    TEST_ASSERT_TRUE(draft.attemptFailed());
    TEST_ASSERT_TRUE(draft.retry());
    assertStage(app::HostDraftStage::Connecting, draft);
    TEST_ASSERT_TRUE(draft.attemptFailed());
    TEST_ASSERT_TRUE(draft.edit());
    assertStage(app::HostDraftStage::Editing, draft);
    TEST_ASSERT_FALSE(draft.retry());
}

void testDiscardClearsAllProfileBytes() {
    app::HostDraftTransaction draft(fixtureProfile(), -1);
    TEST_ASSERT_TRUE(draft.discard());
    assertStage(app::HostDraftStage::Discarded, draft);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&draft.profile());
    for (size_t index = 0; index < sizeof(app::SshHostProfile); ++index) {
        TEST_ASSERT_EQUAL_UINT8(0, bytes[index]);
    }
    TEST_ASSERT_FALSE(draft.beginAttempt());
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testPersistenceGateOpensOnlyAfterAuthentication);
    RUN_TEST(testFailedPersistenceReturnsToRecovery);
    RUN_TEST(testRetryAndEditTransitionsAreBounded);
    RUN_TEST(testDiscardClearsAllProfileBytes);
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
