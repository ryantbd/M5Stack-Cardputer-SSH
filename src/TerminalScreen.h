#pragma once

#include <array>
#include <stddef.h>
#include <stdint.h>

#include "TerminalTypes.h"

namespace app {

enum class TerminalEraseMode : uint8_t { After, Before, All };

class TerminalScreen {
public:
    TerminalScreen();

    void reset();
    const TerminalCell& cell(size_t row, size_t column) const;
    const TerminalCursor& cursor() const;
    void setCursor(int row, int column);
    void moveCursor(int rowDelta, int columnDelta);
    void setCursorVisible(bool visible);
    void setMargins(uint8_t top, uint8_t bottom);
    uint8_t marginTop() const;
    uint8_t marginBottom() const;
    void setStyle(uint8_t foreground, uint8_t background, uint8_t attributes);
    bool putGlyph(uint16_t glyph, bool insertMode, bool autowrap, TerminalRow& removedRow);
    bool lineFeed(TerminalRow& removedRow);
    bool reverseIndex();
    void carriageReturn();
    void tab();
    void backspace();
    void eraseDisplay(TerminalEraseMode mode);
    void eraseLine(TerminalEraseMode mode);
    void insertCharacters(uint8_t count);
    void deleteCharacters(uint8_t count);
    void insertLines(uint8_t count);
    void deleteLines(uint8_t count);
    bool scrollUp(uint8_t count, TerminalRow* firstRemoved);
    void scrollDown(uint8_t count);
    uint16_t dirtyRows() const;
    void clearDirtyRows();

private:
    TerminalCell blankCell() const;
    void blankRow(size_t row);
    void blankCells(size_t row, size_t firstColumn, size_t lastColumn);
    void markRow(size_t row);
    void markRows(size_t firstRow, size_t lastRow);
    static uint8_t clipped(int value, uint8_t maximum);

    std::array<TerminalRow, kTerminalRows> rows_{};
    TerminalCursor cursor_{};
    uint8_t marginTop_ = 0;
    uint8_t marginBottom_ = static_cast<uint8_t>(kTerminalRows - 1);
    uint8_t foreground_ = 7;
    uint8_t background_ = 0;
    uint8_t attributes_ = 0;
    uint16_t dirtyRows_ = 0;
    bool wrapPending_ = false;
};

}  // namespace app
