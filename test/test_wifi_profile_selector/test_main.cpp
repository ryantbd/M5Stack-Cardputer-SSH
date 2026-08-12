#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "WifiProfileSelector.h"

void setUp() {}
void tearDown() {}

namespace {

app::WifiProfile profile(const char* ssid, app::WifiPriority priority, bool hidden = false) {
    app::WifiProfile value{};
    app::copyConfigString(value.ssid, sizeof(value.ssid), ssid);
    value.priority = static_cast<uint8_t>(priority);
    value.hidden = hidden ? 1 : 0;
    value.enabled = 1;
    return value;
}

app::WifiNetworkInfo scan(const char* ssid, int32_t rssi) {
    app::WifiNetworkInfo value{};
    app::copyConfigString(value.ssid, sizeof(value.ssid), ssid);
    value.rssi = rssi;
    return value;
}

void testVisibleCandidatesUsePriorityThenRssiBeforeHidden() {
    app::WifiProfile profiles[4]{};
    profiles[0] = profile("normal-strong", app::WifiPriority::Normal);
    profiles[1] = profile("high-weak", app::WifiPriority::High);
    profiles[2] = profile("hidden-high", app::WifiPriority::High, true);
    const app::WifiNetworkInfo scans[2] = {scan("normal-strong", -30), scan("high-weak", -80)};
    app::WifiCandidate output[4]{};
    const size_t count = app::WifiProfileSelector::build(profiles, 3, scans, 2, output, 4);
    TEST_ASSERT_EQUAL_UINT32(3, count);
    TEST_ASSERT_EQUAL_UINT8(1, output[0].profileIndex);
    TEST_ASSERT_EQUAL_UINT8(0, output[1].profileIndex);
    TEST_ASSERT_EQUAL_UINT8(2, output[2].profileIndex);
}

void testEqualPriorityUsesStrongestThenStableSlot() {
    app::WifiProfile profiles[3] = {
        profile("tie-a", app::WifiPriority::Normal),
        profile("strong", app::WifiPriority::Normal),
        profile("tie-b", app::WifiPriority::Normal),
    };
    const app::WifiNetworkInfo scans[3] = {
        scan("tie-a", -50), scan("strong", -20), scan("tie-b", -50)};
    app::WifiCandidate output[3]{};
    TEST_ASSERT_EQUAL_UINT32(3, app::WifiProfileSelector::build(profiles, 3, scans, 3, output, 3));
    TEST_ASSERT_EQUAL_UINT8(1, output[0].profileIndex);
    TEST_ASSERT_EQUAL_UINT8(0, output[1].profileIndex);
    TEST_ASSERT_EQUAL_UINT8(2, output[2].profileIndex);
}

void testDisabledAndUnseenVisibleProfilesAreSkipped() {
    app::WifiProfile profiles[3] = {
        profile("disabled", app::WifiPriority::High),
        profile("unseen", app::WifiPriority::High),
        profile("hidden", app::WifiPriority::Low, true),
    };
    profiles[0].enabled = 0;
    const app::WifiNetworkInfo scans[1] = {scan("disabled", -10)};
    app::WifiCandidate output[3]{};
    TEST_ASSERT_EQUAL_UINT32(1, app::WifiProfileSelector::build(profiles, 3, scans, 1, output, 3));
    TEST_ASSERT_EQUAL_UINT8(2, output[0].profileIndex);
}

class RecordingConnector final : public app::WifiConnector {
public:
    RecordingConnector(const app::NetworkResult* results, size_t count) : results_(results), count_(count) {}

    app::NetworkResult connect(const app::WifiProfile& profileValue,
                               uint32_t,
                               const app::OperationCallbacks&) override {
        lastProfile_ = &profileValue;
        const size_t resultIndex = attempts_ < count_ ? attempts_ : count_ - 1;
        ++attempts_;
        return results_[resultIndex];
    }

    size_t attempts() const { return attempts_; }
    const app::WifiProfile* lastProfile() const { return lastProfile_; }

private:
    const app::NetworkResult* results_;
    size_t count_;
    size_t attempts_ = 0;
    const app::WifiProfile* lastProfile_ = nullptr;
};

void testRunnerTriesEachCandidateOnceUntilSuccessWithoutMutatingPassword() {
    app::ProfileConfigData config;
    app::resetProfileConfig(config);
    config.wifiCount = 2;
    config.wifi[0] = profile("first", app::WifiPriority::Normal);
    app::copyConfigString(config.wifi[0].password, sizeof(config.wifi[0].password), "fixture-pass");
    config.wifi[1] = profile("second", app::WifiPriority::Normal, true);
    const app::WifiCandidate candidates[2] = {{0, -40, false}, {1, INT32_MIN, true}};
    const app::NetworkResult outcomes[2] = {app::NetworkResult::Timeout, app::NetworkResult::Success};
    RecordingConnector connector(outcomes, 2);
    const app::OperationCallbacks callbacks{};
    TEST_ASSERT_EQUAL(app::NetworkResult::Success,
                      app::tryWifiCandidates(config, candidates, 2, connector, 15000, callbacks));
    TEST_ASSERT_EQUAL_UINT32(2, connector.attempts());
    TEST_ASSERT_EQUAL_STRING("fixture-pass", config.wifi[0].password);
    TEST_ASSERT_EQUAL_PTR(&config.wifi[1], connector.lastProfile());
}

void testRunnerStopsOnCancelAndRejectsNoCandidates() {
    app::ProfileConfigData config;
    app::resetProfileConfig(config);
    const app::NetworkResult result = app::NetworkResult::Success;
    RecordingConnector connector(&result, 1);
    const app::OperationCallbacks callbacks{};
    TEST_ASSERT_EQUAL(app::NetworkResult::Failed,
                      app::tryWifiCandidates(config, nullptr, 0, connector, 15000, callbacks));

    config.wifiCount = 1;
    config.wifi[0] = profile("unit", app::WifiPriority::Normal);
    const app::WifiCandidate candidate{0, -10, false};
    const app::NetworkResult canceled = app::NetworkResult::Canceled;
    RecordingConnector canceledConnector(&canceled, 1);
    TEST_ASSERT_EQUAL(app::NetworkResult::Canceled,
                      app::tryWifiCandidates(config, &candidate, 1, canceledConnector, 15000, callbacks));
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testVisibleCandidatesUsePriorityThenRssiBeforeHidden);
    RUN_TEST(testEqualPriorityUsesStrongestThenStableSlot);
    RUN_TEST(testDisabledAndUnseenVisibleProfilesAreSkipped);
    RUN_TEST(testRunnerTriesEachCandidateOnceUntilSuccessWithoutMutatingPassword);
    RUN_TEST(testRunnerStopsOnCancelAndRejectsNoCandidates);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runTests();
}
void loop() {}
#else
int main(int, char**) { return runTests(); }
#endif
