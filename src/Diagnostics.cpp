#include "Diagnostics.h"

#include <stdio.h>

namespace app {

bool formatDiagnosticLine(const DiagnosticsSnapshot& s, size_t index, char* output, size_t capacity) {
    if (output == nullptr || capacity == 0 || index >= kDiagnosticLineCount) return false;
    int written = -1;
    switch (index) {
        case 0: written = snprintf(output, capacity, "SCAN N%u H%u F%u", s.scanNamed, s.scanHidden, s.scanFailed); break;
        case 1: written = snprintf(output, capacity, "WIFI CH%u RSSI %ld", s.wifiChannel, static_cast<long>(s.rssi)); break;
        case 2: written = snprintf(output, capacity, "KEY R%u C%u", s.rawKeyRow, s.rawKeyColumn); break;
        case 3: written = snprintf(output, capacity, "FN%d Aa%d AA%d C%d A%d O%d", s.fnHeld, s.aaHeld, s.aaLocked, s.ctrlHeld, s.altHeld, s.optHeld); break;
        case 4: written = snprintf(output, capacity, "V2 %s W%u H%u", s.configValid ? "OK" : "BAD", s.wifiCount, s.hostCount); break;
        case 5: written = snprintf(output, capacity, "HEAP %lu P%u SSH%d", static_cast<unsigned long>(s.freeHeap), s.page, s.sessionLive); break;
    }
    if (written < 0 || static_cast<size_t>(written) >= capacity) {
        output[capacity - 1] = '\0';
        return false;
    }
    return true;
}

}  // namespace app
