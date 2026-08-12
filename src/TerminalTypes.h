#pragma once

#include <array>
#include <stdint.h>

#include "TerminalGeometry.h"

namespace app {

constexpr uint8_t kTerminalBold = 0x01;
constexpr uint8_t kTerminalUnderline = 0x02;
constexpr uint8_t kTerminalInverse = 0x04;
constexpr uint8_t kTerminalContinuation = 0x08;

class TerminalCell {
public:
    uint16_t glyph() const {
        return static_cast<uint16_t>(glyphLow_) |
               static_cast<uint16_t>(glyphHigh_) << 8;
    }
    void setGlyph(uint16_t value) {
        glyphLow_ = static_cast<uint8_t>(value & 0xff);
        glyphHigh_ = static_cast<uint8_t>(value >> 8);
    }
    uint8_t foreground() const { return foreground_; }
    uint8_t background() const { return background_; }
    uint8_t attributes() const { return attributes_; }
    void setForeground(uint8_t value) { foreground_ = value; }
    void setBackground(uint8_t value) { background_ = value; }
    void setAttributes(uint8_t value) { attributes_ = value; }

private:
    uint8_t glyphLow_ = ' ';
    uint8_t glyphHigh_ = 0;
    uint8_t foreground_ = 7;
    uint8_t background_ = 0;
    uint8_t attributes_ = 0;
};

static_assert(sizeof(TerminalCell) == 5, "TerminalCell must remain five bytes");

using TerminalRow = std::array<TerminalCell, kTerminalColumns>;

struct TerminalCursor {
    uint8_t row = 0;
    uint8_t column = 0;
    bool visible = true;
};

struct TerminalInputModes {
    bool applicationCursor = false;
};

}  // namespace app
