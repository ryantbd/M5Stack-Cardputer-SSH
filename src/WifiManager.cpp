#include "WifiManager.h"

namespace app {

NetworkResult WifiManager::connect(const WifiProfile& profile,
                                   uint32_t timeoutMs,
                                   const OperationCallbacks& callbacks) {
    return connect(profile.ssid, profile.password, timeoutMs, callbacks);
}

WifiScanResult WifiManager::scan(WifiScanModel& model, const OperationCallbacks& callbacks) const {
    model.reset();
    WiFi.mode(WIFI_STA);
    WiFi.scanDelete();
    callbacks.report("Scanning Wi-Fi");
    for (uint8_t channel = kFirstWifiChannel; channel <= kLastWifiChannel; ++channel) {
        if (callbacks.isCanceled()) {
            WiFi.scanDelete();
            model.cancel();
            return WifiScanResult::Canceled;
        }
        callbacks.report("Scanning Wi-Fi", static_cast<uint16_t>(channel - 1), kLastWifiChannel);
        if (!model.beginChannel(channel)) {
            WiFi.scanDelete();
            return WifiScanResult::Failed;
        }

        const int16_t found = WiFi.scanNetworks(false, true, false, 180, channel);
        if (found >= 0) {
            for (int16_t index = 0; index < found; ++index) {
                const String ssid = WiFi.SSID(static_cast<uint8_t>(index));
                const uint8_t* bssid = WiFi.BSSID(static_cast<uint8_t>(index));
                const WifiScanObservation observation{
                    ssid.c_str(),
                    bssid,
                    WiFi.RSSI(static_cast<uint8_t>(index)),
                    static_cast<uint8_t>(WiFi.channel(static_cast<uint8_t>(index))),
                    static_cast<uint8_t>(WiFi.encryptionType(static_cast<uint8_t>(index))),
                };
                model.add(observation);
            }
        }
        model.finishChannel(found >= 0);
        WiFi.scanDelete();
        delay(1);
    }
    callbacks.report("Scanning Wi-Fi", kLastWifiChannel, kLastWifiChannel);
    return model.summary().allChannelsFailed() ? WifiScanResult::Failed : WifiScanResult::Success;
}

NetworkResult WifiManager::connect(const char* ssid,
                                   const char* password,
                                   uint32_t timeoutMs,
                                   const OperationCallbacks& callbacks) const {
    if (ssid == nullptr || ssid[0] == '\0' || password == nullptr) {
        return NetworkResult::Failed;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(false, false);
    delay(20);
    WiFi.begin(ssid, password);

    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < timeoutMs) {
        if (callbacks.isCanceled()) {
            WiFi.disconnect(false, false);
            return NetworkResult::Canceled;
        }
        callbacks.report("Connecting Wi-Fi",
                         static_cast<uint16_t>((millis() - startedAt) / 250U),
                         static_cast<uint16_t>(timeoutMs / 250U));
        delay(50);
    }

    if (WiFi.status() == WL_CONNECTED) {
        return NetworkResult::Success;
    }
    WiFi.disconnect(false, false);
    return NetworkResult::Timeout;
}

void WifiManager::disconnect() const {
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
}

}  // namespace app
