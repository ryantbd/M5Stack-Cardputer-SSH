#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "AppConfig.h"
#include "OperationCallbacks.h"
#include "WifiScanModel.h"
#include "WifiProfileSelector.h"

namespace app {

enum class WifiScanResult : int8_t { Success = 0, Canceled = -1, Failed = -2 };

class WifiManager final : public WifiConnector {
public:
    WifiScanResult scan(WifiScanModel& model, const OperationCallbacks& callbacks) const;
    NetworkResult connect(const WifiProfile& profile,
                          uint32_t timeoutMs,
                          const OperationCallbacks& callbacks) override;
    NetworkResult connect(const char* ssid,
                          const char* password,
                          uint32_t timeoutMs,
                          const OperationCallbacks& callbacks) const;
    void disconnect() const;
};

}  // namespace app
