#include "SshDiscovery.h"

#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#endif

namespace app {

void SshDiscoveryList::reset() {
    secureClear(results_, sizeof(results_));
    count_ = 0;
}

bool SshDiscoveryList::add(const char* name, const char* target, uint16_t port) {
    if (target == nullptr || target[0] == '\0' || count_ >= kMaxSshDiscoveryResults) return false;
    char normalized[kSshHostCapacity]{};
    const bool dotted = strchr(target, '.') != nullptr || strchr(target, ':') != nullptr;
    const int written = snprintf(normalized, sizeof(normalized), dotted ? "%s" : "%s.local", target);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(normalized)) return false;
    if (port == 0) port = kDefaultSshPort;
    for (size_t index = 0; index < count_; ++index) {
        if (results_[index].port == port && strcmp(results_[index].target, normalized) == 0) return false;
    }
    SshServiceInfo candidate{};
    if (!copyConfigString(candidate.target, sizeof(candidate.target), normalized) ||
        !copyConfigString(candidate.name, sizeof(candidate.name),
                          name == nullptr || name[0] == '\0' ? normalized : name)) {
        secureClear(&candidate, sizeof(candidate));
        return false;
    }
    candidate.port = port;
    results_[count_++] = candidate;
    secureClear(&candidate, sizeof(candidate));
    return true;
}

int SshDiscovery::discover(SshServiceInfo* results,
                           size_t capacity,
                           const OperationCallbacks& callbacks) {
#ifdef ARDUINO
    if (results == nullptr || capacity == 0 || WiFi.status() != WL_CONNECTED) return -1;
    if (callbacks.isCanceled()) return -2;
    callbacks.report("Discovering SSH", 0, 1);
    SshDiscoveryList list;
    if (ensureMdns()) {
        const int services = MDNS.queryService("ssh", "tcp");
        for (int index = 0; index < services && list.count() < kMaxSshDiscoveryResults; ++index) {
            const String hostname = MDNS.hostname(index);
            const String ip = MDNS.IP(index).toString();
            const char* target = hostname.isEmpty() ? ip.c_str() : hostname.c_str();
            list.add(hostname.c_str(), target, MDNS.port(index));
        }
    }
    if (callbacks.isCanceled()) return -2;
    const size_t count = min(capacity, list.count());
    for (size_t index = 0; index < count; ++index) results[index] = list.results()[index];
    callbacks.report("Discovering SSH", 1, 1);
    return static_cast<int>(count);
#else
    (void)results;
    (void)capacity;
    (void)callbacks;
    return -1;
#endif
}

bool SshDiscovery::ensureMdns() {
#ifdef ARDUINO
    if (mdnsStarted_) return true;
    const uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFU);
    const String hostname = "cardputer-ssh-" + String(suffix, HEX);
    mdnsStarted_ = MDNS.begin(hostname.c_str());
    return mdnsStarted_;
#else
    return false;
#endif
}

}  // namespace app
