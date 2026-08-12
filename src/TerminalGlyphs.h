#pragma once

#include <stdint.h>

namespace app {

enum class TerminalGlyphKind : uint8_t {
    Builtin,
    Latin1,
    Arrow,
    BoxDrawing,
    Block,
    Geometric,
    Braille,
    Replacement,
};

constexpr uint16_t kTerminalReplacementGlyph = 0xfffd;

uint16_t normalizeTerminalCodePoint(uint32_t codePoint);
TerminalGlyphKind terminalGlyphKind(uint16_t glyph);
bool terminalGlyphMask(uint16_t glyph, uint8_t columns[5]);

}  // namespace app
