#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <cstring>

#include "WifiScanModel.h"

void setUp() {}
void tearDown() {}

namespace {

void testSameBssidAndSsidKeepsStrongestObservation() {
    app::WifiScanModel model;
    const uint8_t bssid[6] = {0, 1, 2, 3, 4, 5};
    TEST_ASSERT_TRUE(model.beginChannel(1));
    TEST_ASSERT_TRUE(model.add(app::WifiScanObservation{"unit-net", bssid, -72, 1, 3}));
    TEST_ASSERT_TRUE(model.add(app::WifiScanObservation{"unit-net", bssid, -41, 1, 3}));
    TEST_ASSERT_TRUE(model.finishChannel(true));
    TEST_ASSERT_EQUAL_UINT32(1, model.summary().namedCount);
    TEST_ASSERT_EQUAL_INT32(-41, model.results()[0].rssi);
}

void testDistinctTuplesAndDescendingSignalAreRetained() {
    app::WifiScanModel model;
    const uint8_t first[6] = {0, 1, 2, 3, 4, 5};
    const uint8_t second[6] = {6, 7, 8, 9, 10, 11};
    TEST_ASSERT_TRUE(model.beginChannel(1));
    TEST_ASSERT_TRUE(model.add(app::WifiScanObservation{"weak", first, -80, 1, 3}));
    TEST_ASSERT_TRUE(model.add(app::WifiScanObservation{"strong", second, -25, 1, 0}));
    TEST_ASSERT_TRUE(model.add(app::WifiScanObservation{"renamed", first, -40, 1, 3}));
    TEST_ASSERT_EQUAL_UINT32(3, model.resultCount());
    TEST_ASSERT_EQUAL_STRING("strong", model.results()[0].ssid);
    TEST_ASSERT_EQUAL_STRING("renamed", model.results()[1].ssid);
}

void testHiddenAndInvalidNamesStayOutOfSelectableResults() {
    app::WifiScanModel model;
    const uint8_t bssid[6] = {0, 1, 2, 3, 4, 5};
    char tooLong[app::kWifiSsidCapacity + 1];
    std::memset(tooLong, 'x', sizeof(tooLong));
    tooLong[sizeof(tooLong) - 1] = '\0';
    TEST_ASSERT_TRUE(model.beginChannel(1));
    TEST_ASSERT_TRUE(model.add(app::WifiScanObservation{"", bssid, -20, 1, 3}));
    TEST_ASSERT_FALSE(model.add(app::WifiScanObservation{tooLong, bssid, -20, 1, 3}));
    TEST_ASSERT_EQUAL_UINT16(1, model.summary().hiddenCount);
    TEST_ASSERT_EQUAL_UINT16(1, model.summary().droppedCount);
    TEST_ASSERT_EQUAL_UINT32(0, model.resultCount());
}

void testPartialChannelFailureStillSucceeds() {
    app::WifiScanModel model;
    for (uint8_t channel = 1; channel <= 13; ++channel) {
        TEST_ASSERT_TRUE(model.beginChannel(channel));
        TEST_ASSERT_TRUE(model.finishChannel(channel != 6));
    }
    const auto summary = model.summary();
    TEST_ASSERT_EQUAL_UINT8(13, summary.completedChannels);
    TEST_ASSERT_EQUAL_UINT8(1, summary.failedChannels);
    TEST_ASSERT_FALSE(summary.allChannelsFailed());
}

void testAllChannelFailuresAreReported() {
    app::WifiScanModel model;
    for (uint8_t channel = 1; channel <= 13; ++channel) {
        TEST_ASSERT_TRUE(model.beginChannel(channel));
        TEST_ASSERT_TRUE(model.finishChannel(false));
    }
    TEST_ASSERT_TRUE(model.summary().allChannelsFailed());
}

void testWrongOrderAndCancellationStopFurtherInput() {
    app::WifiScanModel model;
    const uint8_t bssid[6] = {};
    TEST_ASSERT_FALSE(model.beginChannel(2));
    TEST_ASSERT_TRUE(model.beginChannel(1));
    TEST_ASSERT_TRUE(model.finishChannel(true));
    model.cancel();
    TEST_ASSERT_FALSE(model.beginChannel(2));
    TEST_ASSERT_FALSE(model.add(app::WifiScanObservation{"ignored", bssid, -20, 2, 0}));
    TEST_ASSERT_TRUE(model.summary().canceled);
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testSameBssidAndSsidKeepsStrongestObservation);
    RUN_TEST(testDistinctTuplesAndDescendingSignalAreRetained);
    RUN_TEST(testHiddenAndInvalidNamesStayOutOfSelectableResults);
    RUN_TEST(testPartialChannelFailureStillSucceeds);
    RUN_TEST(testAllChannelFailuresAreReported);
    RUN_TEST(testWrongOrderAndCancellationStopFurtherInput);
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
