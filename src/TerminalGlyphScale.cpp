#include "TerminalGlyphScale.h"

namespace app {

TerminalGlyphSpan scaleTerminalGlyphSpan(uint8_t offset, uint8_t length,
                                         uint8_t sourceExtent,
                                         uint8_t targetExtent) {
    if (length == 0 || sourceExtent == 0 || targetExtent == 0 ||
        offset >= sourceExtent) {
        return {};
    }

    const uint16_t requestedEnd = static_cast<uint16_t>(offset) + length;
    const uint8_t sourceEnd = static_cast<uint8_t>(
        requestedEnd < sourceExtent ? requestedEnd : sourceExtent);
    const uint16_t rounding = sourceExtent / 2U;
    const uint8_t first = static_cast<uint8_t>(
        (static_cast<uint16_t>(offset) * targetExtent + rounding) /
        sourceExtent);
    uint8_t after = static_cast<uint8_t>(
        (static_cast<uint16_t>(sourceEnd) * targetExtent + rounding) /
        sourceExtent);
    if (after <= first) after = static_cast<uint8_t>(first + 1U);
    if (after > targetExtent) after = targetExtent;
    TerminalGlyphSpan result;
    result.offset = first;
    result.length = static_cast<uint8_t>(after - first);
    return result;
}

}  // namespace app
