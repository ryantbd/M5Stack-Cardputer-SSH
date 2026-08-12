#include "WifiProfileSelector.h"

#include <string.h>

namespace app {

namespace {

bool comesBefore(const WifiCandidate& left,
                 const WifiCandidate& right,
                 const WifiProfile* profiles) {
    const uint8_t leftPriority = profiles[left.profileIndex].priority;
    const uint8_t rightPriority = profiles[right.profileIndex].priority;
    if (leftPriority != rightPriority) {
        return leftPriority > rightPriority;
    }
    if (!left.hidden && left.rssi != right.rssi) {
        return left.rssi > right.rssi;
    }
    return left.profileIndex < right.profileIndex;
}

void insertCandidate(WifiCandidate* values,
                     size_t& count,
                     const WifiCandidate& candidate,
                     const WifiProfile* profiles) {
    size_t position = count;
    while (position > 0 && comesBefore(candidate, values[position - 1], profiles)) {
        values[position] = values[position - 1];
        --position;
    }
    values[position] = candidate;
    ++count;
}

}  // namespace

size_t WifiProfileSelector::build(const WifiProfile* profiles,
                                  size_t profileCount,
                                  const WifiNetworkInfo* scans,
                                  size_t scanCount,
                                  WifiCandidate* output,
                                  size_t outputCapacity) {
    if (profiles == nullptr || output == nullptr || outputCapacity == 0 || profileCount > kMaxWifiProfiles ||
        (scanCount > 0 && scans == nullptr)) {
        return 0;
    }
    for (size_t index = 0; index < outputCapacity; ++index) {
        output[index] = WifiCandidate{};
    }

    WifiCandidate visible[kMaxWifiProfiles]{};
    WifiCandidate hidden[kMaxWifiProfiles]{};
    size_t visibleCount = 0;
    size_t hiddenCount = 0;

    for (size_t profileIndex = 0; profileIndex < profileCount; ++profileIndex) {
        const WifiProfile& profile = profiles[profileIndex];
        if (profile.enabled == 0 || profile.ssid[0] == '\0') {
            continue;
        }
        if (profile.hidden != 0) {
            const WifiCandidate candidate{static_cast<uint8_t>(profileIndex), INT32_MIN, true};
            insertCandidate(hidden, hiddenCount, candidate, profiles);
            continue;
        }

        bool seen = false;
        int32_t strongest = INT32_MIN;
        for (size_t scanIndex = 0; scanIndex < scanCount; ++scanIndex) {
            if (strcmp(profile.ssid, scans[scanIndex].ssid) == 0 && (!seen || scans[scanIndex].rssi > strongest)) {
                seen = true;
                strongest = scans[scanIndex].rssi;
            }
        }
        if (seen) {
            const WifiCandidate candidate{static_cast<uint8_t>(profileIndex), strongest, false};
            insertCandidate(visible, visibleCount, candidate, profiles);
        }
    }

    size_t outputCount = 0;
    for (size_t index = 0; index < visibleCount && outputCount < outputCapacity; ++index) {
        output[outputCount++] = visible[index];
    }
    for (size_t index = 0; index < hiddenCount && outputCount < outputCapacity; ++index) {
        output[outputCount++] = hidden[index];
    }
    secureClear(visible, sizeof(visible));
    secureClear(hidden, sizeof(hidden));
    return outputCount;
}

NetworkResult tryWifiCandidates(const ProfileConfigData& config,
                                const WifiCandidate* candidates,
                                size_t candidateCount,
                                WifiConnector& connector,
                                uint32_t timeoutMs,
                                const OperationCallbacks& callbacks) {
    if (candidates == nullptr || candidateCount == 0) {
        return NetworkResult::Failed;
    }

    NetworkResult lastResult = NetworkResult::Failed;
    bool attempted = false;
    for (size_t index = 0; index < candidateCount; ++index) {
        if (callbacks.isCanceled()) {
            return NetworkResult::Canceled;
        }
        const uint8_t profileIndex = candidates[index].profileIndex;
        if (profileIndex >= config.wifiCount || profileIndex >= kMaxWifiProfiles ||
            config.wifi[profileIndex].enabled == 0) {
            continue;
        }
        callbacks.report("Connecting Wi-Fi", static_cast<uint16_t>(index + 1),
                         static_cast<uint16_t>(candidateCount));
        attempted = true;
        lastResult = connector.connect(config.wifi[profileIndex], timeoutMs, callbacks);
        if (lastResult == NetworkResult::Success || lastResult == NetworkResult::Canceled) {
            return lastResult;
        }
    }
    return attempted ? lastResult : NetworkResult::Failed;
}

}  // namespace app
