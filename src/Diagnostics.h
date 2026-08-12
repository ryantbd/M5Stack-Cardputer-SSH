#pragma once

#include <stddef.h>
#include <stdint.h>

namespace app {

struct DiagnosticsSnapshot {
    uint16_t scanNamed = 0;
    uint16_t scanHidden = 0;
    uint8_t scanFailed = 0;
    int32_t rssi = 0;
    uint8_t wifiChannel = 0;
    uint8_t rawKeyRow = 0;
    uint8_t rawKeyColumn = 0;
    bool fnHeld = false;
    bool aaHeld = false;
    bool aaLocked = false;
    bool ctrlHeld = false;
    bool altHeld = false;
    bool optHeld = false;
    bool configValid = false;
    uint8_t wifiCount = 0;
    uint8_t hostCount = 0;
    uint32_t freeHeap = 0;
    uint8_t page = 0;
    bool sessionLive = false;
};

constexpr size_t kDiagnosticLineCount = 6;
bool formatDiagnosticLine(const DiagnosticsSnapshot& snapshot, size_t lineIndex, char* output, size_t capacity);

}  // namespace app
