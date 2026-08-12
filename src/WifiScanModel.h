#pragma once

#include <stddef.h>
#include <stdint.h>

#include "AppConfig.h"

namespace app {

constexpr size_t kMaxWifiResults = 20;
constexpr uint8_t kFirstWifiChannel = 1;
constexpr uint8_t kLastWifiChannel = 13;

struct WifiScanObservation {
    const char* ssid;
    const uint8_t* bssid;
    int32_t rssi;
    uint8_t channel;
    uint8_t encryption;
};

struct WifiNetworkInfo {
    char ssid[kWifiSsidCapacity]{};
    uint8_t bssid[6]{};
    int32_t rssi = 0;
    uint8_t channel = 0;
    uint8_t encryption = 0;

    bool isOpen() const { return encryption == 0; }
};

struct WifiScanSummary {
    uint8_t completedChannels = 0;
    uint8_t failedChannels = 0;
    uint16_t namedCount = 0;
    uint16_t hiddenCount = 0;
    uint16_t droppedCount = 0;
    bool canceled = false;

    bool allChannelsFailed() const {
        return completedChannels == kLastWifiChannel && failedChannels == completedChannels;
    }
};

class WifiScanModel {
public:
    WifiScanModel();

    void reset();
    bool beginChannel(uint8_t channel);
    bool add(const WifiScanObservation& observation);
    bool finishChannel(bool success);
    void cancel();

    const WifiNetworkInfo* results() const { return results_; }
    size_t resultCount() const { return resultCount_; }
    const WifiScanSummary& summary() const { return summary_; }

private:
    void sortBySignal();

    WifiNetworkInfo results_[kMaxWifiResults]{};
    size_t resultCount_ = 0;
    WifiScanSummary summary_{};
    uint8_t expectedChannel_ = kFirstWifiChannel;
    bool channelActive_ = false;
};

}  // namespace app
