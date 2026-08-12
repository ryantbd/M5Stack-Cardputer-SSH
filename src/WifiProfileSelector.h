#pragma once

#include <stddef.h>
#include <stdint.h>

#include "AppConfig.h"
#include "OperationCallbacks.h"
#include "WifiScanModel.h"

namespace app {

enum class NetworkResult : uint8_t { Success, Canceled, Timeout, Failed };

struct WifiCandidate {
    constexpr WifiCandidate(uint8_t index = 0, int32_t signal = INT32_MIN, bool isHidden = false)
        : profileIndex(index), rssi(signal), hidden(isHidden) {}

    uint8_t profileIndex;
    int32_t rssi;
    bool hidden;
};

class WifiProfileSelector {
public:
    static size_t build(const WifiProfile* profiles,
                        size_t profileCount,
                        const WifiNetworkInfo* scans,
                        size_t scanCount,
                        WifiCandidate* output,
                        size_t outputCapacity);
};

class WifiConnector {
public:
    virtual ~WifiConnector() = default;
    virtual NetworkResult connect(const WifiProfile& profile,
                                  uint32_t timeoutMs,
                                  const OperationCallbacks& callbacks) = 0;
};

NetworkResult tryWifiCandidates(const ProfileConfigData& config,
                                const WifiCandidate* candidates,
                                size_t candidateCount,
                                WifiConnector& connector,
                                uint32_t timeoutMs,
                                const OperationCallbacks& callbacks);

}  // namespace app
