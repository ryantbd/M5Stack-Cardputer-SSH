#pragma once

#include <stdint.h>

namespace app {

constexpr uint8_t kTerminalGlyphSourceWidth = 5;
constexpr uint8_t kTerminalGlyphSourceHeight = 8;

struct TerminalGlyphSpan {
    uint8_t offset = 0;
    uint8_t length = 0;
};

TerminalGlyphSpan scaleTerminalGlyphSpan(uint8_t offset, uint8_t length,
                                         uint8_t sourceExtent,
                                         uint8_t targetExtent);

}  // namespace app
