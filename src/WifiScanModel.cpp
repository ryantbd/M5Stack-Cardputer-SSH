#include "WifiScanModel.h"

#include <string.h>

namespace app {

WifiScanModel::WifiScanModel() {
    reset();
}

void WifiScanModel::reset() {
    secureClear(results_, sizeof(results_));
    resultCount_ = 0;
    summary_ = WifiScanSummary{};
    expectedChannel_ = kFirstWifiChannel;
    channelActive_ = false;
}

bool WifiScanModel::beginChannel(uint8_t channel) {
    if (summary_.canceled || channelActive_ || channel != expectedChannel_ || channel > kLastWifiChannel) {
        return false;
    }
    channelActive_ = true;
    return true;
}

bool WifiScanModel::add(const WifiScanObservation& observation) {
    if (!channelActive_ || summary_.canceled || observation.ssid == nullptr || observation.bssid == nullptr ||
        observation.channel != expectedChannel_) {
        return false;
    }

    const size_t ssidLength = strnlen(observation.ssid, kWifiSsidCapacity);
    if (ssidLength == 0) {
        ++summary_.hiddenCount;
        return true;
    }
    if (ssidLength >= kWifiSsidCapacity) {
        ++summary_.droppedCount;
        return false;
    }

    for (size_t index = 0; index < resultCount_; ++index) {
        WifiNetworkInfo& existing = results_[index];
        if (memcmp(existing.bssid, observation.bssid, sizeof(existing.bssid)) == 0 &&
            strcmp(existing.ssid, observation.ssid) == 0) {
            if (observation.rssi > existing.rssi) {
                existing.rssi = observation.rssi;
                existing.channel = observation.channel;
                existing.encryption = observation.encryption;
                sortBySignal();
            }
            return true;
        }
    }

    if (resultCount_ >= kMaxWifiResults) {
        ++summary_.droppedCount;
        return false;
    }

    WifiNetworkInfo& added = results_[resultCount_++];
    secureClear(&added, sizeof(added));
    copyConfigString(added.ssid, sizeof(added.ssid), observation.ssid);
    memcpy(added.bssid, observation.bssid, sizeof(added.bssid));
    added.rssi = observation.rssi;
    added.channel = observation.channel;
    added.encryption = observation.encryption;
    summary_.namedCount = static_cast<uint16_t>(resultCount_);
    sortBySignal();
    return true;
}

bool WifiScanModel::finishChannel(bool success) {
    if (!channelActive_ || summary_.canceled) {
        return false;
    }
    channelActive_ = false;
    ++summary_.completedChannels;
    if (!success) {
        ++summary_.failedChannels;
    }
    ++expectedChannel_;
    return true;
}

void WifiScanModel::cancel() {
    summary_.canceled = true;
    channelActive_ = false;
}

void WifiScanModel::sortBySignal() {
    for (size_t index = 1; index < resultCount_; ++index) {
        const WifiNetworkInfo value = results_[index];
        size_t position = index;
        while (position > 0 && results_[position - 1].rssi < value.rssi) {
            results_[position] = results_[position - 1];
            --position;
        }
        results_[position] = value;
    }
}

}  // namespace app
