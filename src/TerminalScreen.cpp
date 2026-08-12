#include "TerminalScreen.h"

#include <stdint.h>

namespace app {
namespace {

constexpr uint16_t allRowsDirtyMask() {
    return static_cast<uint16_t>((static_cast<uint32_t>(1) << kTerminalRows) - 1);
}

uint8_t vtCount(uint8_t count, uint8_t maximum) {
    const uint8_t requested = count == 0 ? 1 : count;
    return requested > maximum ? maximum : requested;
}

}  // namespace

TerminalScreen::TerminalScreen() { reset(); }

void TerminalScreen::reset() {
    rows_ = {};
    cursor_ = {};
    marginTop_ = 0;
    marginBottom_ = static_cast<uint8_t>(kTerminalRows - 1);
    foreground_ = 7;
    background_ = 0;
    attributes_ = 0;
    dirtyRows_ = allRowsDirtyMask();
    wrapPending_ = false;
}

const TerminalCell& TerminalScreen::cell(size_t row, size_t column) const {
    static const TerminalCell blank;
    if (row >= kTerminalRows || column >= kTerminalColumns) return blank;
    return rows_[row][column];
}

const TerminalCursor& TerminalScreen::cursor() const { return cursor_; }

uint8_t TerminalScreen::clipped(int value, uint8_t maximum) {
    if (value < 0) return 0;
    if (value > maximum) return maximum;
    return static_cast<uint8_t>(value);
}

void TerminalScreen::setCursor(int row, int column) {
    markRow(cursor_.row);
    cursor_.row = clipped(row, static_cast<uint8_t>(kTerminalRows - 1));
    cursor_.column = clipped(column, static_cast<uint8_t>(kTerminalColumns - 1));
    markRow(cursor_.row);
    wrapPending_ = false;
}

void TerminalScreen::moveCursor(int rowDelta, int columnDelta) {
    const int64_t row = static_cast<int64_t>(cursor_.row) + rowDelta;
    const int64_t column = static_cast<int64_t>(cursor_.column) + columnDelta;
    const int clippedRow = row < 0 ? 0 : row >= static_cast<int64_t>(kTerminalRows)
                                              ? static_cast<int>(kTerminalRows - 1)
                                              : static_cast<int>(row);
    const int clippedColumn = column < 0 ? 0 : column >= static_cast<int64_t>(kTerminalColumns)
                                                    ? static_cast<int>(kTerminalColumns - 1)
                                                    : static_cast<int>(column);
    setCursor(clippedRow, clippedColumn);
}

void TerminalScreen::setCursorVisible(bool visible) {
    if (cursor_.visible == visible) return;
    cursor_.visible = visible;
    markRow(cursor_.row);
}

void TerminalScreen::setMargins(uint8_t top, uint8_t bottom) {
    if (top >= kTerminalRows || bottom >= kTerminalRows || top >= bottom) return;
    marginTop_ = top;
    marginBottom_ = bottom;
    wrapPending_ = false;
}

uint8_t TerminalScreen::marginTop() const { return marginTop_; }

uint8_t TerminalScreen::marginBottom() const { return marginBottom_; }

void TerminalScreen::setStyle(uint8_t foreground, uint8_t background, uint8_t attributes) {
    foreground_ = foreground;
    background_ = background;
    attributes_ = attributes;
}

TerminalCell TerminalScreen::blankCell() const {
    TerminalCell cell;
    cell.setForeground(foreground_);
    cell.setBackground(background_);
    cell.setAttributes(attributes_);
    return cell;
}

void TerminalScreen::blankRow(size_t row) {
    if (row >= kTerminalRows) return;
    rows_[row].fill(blankCell());
}

void TerminalScreen::blankCells(size_t row, size_t firstColumn, size_t lastColumn) {
    if (row >= kTerminalRows || firstColumn >= kTerminalColumns || firstColumn > lastColumn) {
        return;
    }
    if (lastColumn >= kTerminalColumns) lastColumn = kTerminalColumns - 1;
    const TerminalCell blank = blankCell();
    for (size_t column = firstColumn; column <= lastColumn; ++column) {
        rows_[row][column] = blank;
    }
    markRow(row);
}

void TerminalScreen::markRow(size_t row) {
    if (row < kTerminalRows) dirtyRows_ |= static_cast<uint16_t>(1u << row);
}

void TerminalScreen::markRows(size_t firstRow, size_t lastRow) {
    if (firstRow >= kTerminalRows || firstRow > lastRow) return;
    if (lastRow >= kTerminalRows) lastRow = kTerminalRows - 1;
    for (size_t row = firstRow; row <= lastRow; ++row) markRow(row);
}

bool TerminalScreen::putGlyph(uint16_t glyph, bool insertMode, bool autowrap,
                              TerminalRow& removedRow) {
    bool removed = false;
    if (wrapPending_) {
        wrapPending_ = false;
        if (autowrap) {
            cursor_.column = 0;
            removed = lineFeed(removedRow);
        }
    }

    if (insertMode) insertCharacters(1);

    TerminalCell& destination = rows_[cursor_.row][cursor_.column];
    destination.setGlyph(glyph);
    destination.setForeground(foreground_);
    destination.setBackground(background_);
    destination.setAttributes(attributes_);
    markRow(cursor_.row);

    if (cursor_.column == kTerminalColumns - 1) {
        wrapPending_ = autowrap;
    } else {
        ++cursor_.column;
    }
    return removed;
}

bool TerminalScreen::lineFeed(TerminalRow& removedRow) {
    wrapPending_ = false;
    if (cursor_.row == marginBottom_) {
        return scrollUp(1, &removedRow);
    }
    if (cursor_.row < kTerminalRows - 1) {
        const uint8_t oldRow = cursor_.row;
        ++cursor_.row;
        markRow(oldRow);
        markRow(cursor_.row);
    }
    return false;
}

bool TerminalScreen::reverseIndex() {
    wrapPending_ = false;
    if (cursor_.row == marginTop_) {
        scrollDown(1);
        return true;
    }
    if (cursor_.row == 0) return false;

    const uint8_t oldRow = cursor_.row;
    --cursor_.row;
    markRow(oldRow);
    markRow(cursor_.row);
    return true;
}

void TerminalScreen::carriageReturn() {
    wrapPending_ = false;
    markRow(cursor_.row);
    cursor_.column = 0;
}

void TerminalScreen::tab() {
    wrapPending_ = false;
    markRow(cursor_.row);
    const unsigned int next = (static_cast<unsigned int>(cursor_.column) / 8u + 1u) * 8u;
    cursor_.column = next >= kTerminalColumns ? static_cast<uint8_t>(kTerminalColumns - 1)
                                              : static_cast<uint8_t>(next);
}

void TerminalScreen::backspace() {
    wrapPending_ = false;
    markRow(cursor_.row);
    if (cursor_.column > 0) --cursor_.column;
}

void TerminalScreen::eraseDisplay(TerminalEraseMode mode) {
    wrapPending_ = false;
    switch (mode) {
        case TerminalEraseMode::After:
            blankCells(cursor_.row, cursor_.column, kTerminalColumns - 1);
            for (size_t row = cursor_.row + 1; row < kTerminalRows; ++row) blankRow(row);
            markRows(cursor_.row, kTerminalRows - 1);
            break;
        case TerminalEraseMode::Before:
            for (size_t row = 0; row < cursor_.row; ++row) blankRow(row);
            blankCells(cursor_.row, 0, cursor_.column);
            markRows(0, cursor_.row);
            break;
        case TerminalEraseMode::All:
            for (size_t row = 0; row < kTerminalRows; ++row) blankRow(row);
            dirtyRows_ = allRowsDirtyMask();
            break;
    }
}

void TerminalScreen::eraseLine(TerminalEraseMode mode) {
    wrapPending_ = false;
    switch (mode) {
        case TerminalEraseMode::After:
            blankCells(cursor_.row, cursor_.column, kTerminalColumns - 1);
            break;
        case TerminalEraseMode::Before:
            blankCells(cursor_.row, 0, cursor_.column);
            break;
        case TerminalEraseMode::All:
            blankRow(cursor_.row);
            markRow(cursor_.row);
            break;
    }
}

void TerminalScreen::insertCharacters(uint8_t count) {
    wrapPending_ = false;
    const uint8_t available = static_cast<uint8_t>(kTerminalColumns - cursor_.column);
    const uint8_t amount = vtCount(count, available);
    for (int column = static_cast<int>(kTerminalColumns) - 1;
         column >= static_cast<int>(cursor_.column + amount); --column) {
        rows_[cursor_.row][static_cast<size_t>(column)] =
            rows_[cursor_.row][static_cast<size_t>(column - amount)];
    }
    const TerminalCell blank = blankCell();
    for (uint8_t column = cursor_.column; column < cursor_.column + amount; ++column) {
        rows_[cursor_.row][column] = blank;
    }
    markRow(cursor_.row);
}

void TerminalScreen::deleteCharacters(uint8_t count) {
    wrapPending_ = false;
    const uint8_t available = static_cast<uint8_t>(kTerminalColumns - cursor_.column);
    const uint8_t amount = vtCount(count, available);
    size_t column = cursor_.column;
    for (; column + amount < kTerminalColumns; ++column) {
        rows_[cursor_.row][column] = rows_[cursor_.row][column + amount];
    }
    const TerminalCell blank = blankCell();
    for (; column < kTerminalColumns; ++column) rows_[cursor_.row][column] = blank;
    markRow(cursor_.row);
}

void TerminalScreen::insertLines(uint8_t count) {
    wrapPending_ = false;
    if (cursor_.row < marginTop_ || cursor_.row > marginBottom_) return;
    const uint8_t available = static_cast<uint8_t>(marginBottom_ - cursor_.row + 1);
    const uint8_t amount = vtCount(count, available);
    for (int row = marginBottom_; row >= static_cast<int>(cursor_.row + amount); --row) {
        rows_[static_cast<size_t>(row)] = rows_[static_cast<size_t>(row - amount)];
    }
    for (uint8_t row = cursor_.row; row < cursor_.row + amount; ++row) blankRow(row);
    markRows(cursor_.row, marginBottom_);
}

void TerminalScreen::deleteLines(uint8_t count) {
    wrapPending_ = false;
    if (cursor_.row < marginTop_ || cursor_.row > marginBottom_) return;
    const uint8_t available = static_cast<uint8_t>(marginBottom_ - cursor_.row + 1);
    const uint8_t amount = vtCount(count, available);
    size_t row = cursor_.row;
    for (; row + amount <= marginBottom_; ++row) rows_[row] = rows_[row + amount];
    for (; row <= marginBottom_; ++row) blankRow(row);
    markRows(cursor_.row, marginBottom_);
}

bool TerminalScreen::scrollUp(uint8_t count, TerminalRow* firstRemoved) {
    wrapPending_ = false;
    const uint8_t height = static_cast<uint8_t>(marginBottom_ - marginTop_ + 1);
    const uint8_t amount = vtCount(count, height);
    const bool fullScreen = marginTop_ == 0 && marginBottom_ == kTerminalRows - 1;
    if (fullScreen && firstRemoved != nullptr) *firstRemoved = rows_[marginTop_];

    size_t row = marginTop_;
    for (; row + amount <= marginBottom_; ++row) rows_[row] = rows_[row + amount];
    for (; row <= marginBottom_; ++row) blankRow(row);
    markRows(marginTop_, marginBottom_);
    return fullScreen;
}

void TerminalScreen::scrollDown(uint8_t count) {
    wrapPending_ = false;
    const uint8_t height = static_cast<uint8_t>(marginBottom_ - marginTop_ + 1);
    const uint8_t amount = vtCount(count, height);
    for (int row = marginBottom_; row >= static_cast<int>(marginTop_ + amount); --row) {
        rows_[static_cast<size_t>(row)] = rows_[static_cast<size_t>(row - amount)];
    }
    for (uint8_t row = marginTop_; row < marginTop_ + amount; ++row) blankRow(row);
    markRows(marginTop_, marginBottom_);
}

uint16_t TerminalScreen::dirtyRows() const { return dirtyRows_; }

void TerminalScreen::clearDirtyRows() { dirtyRows_ = 0; }

}  // namespace app
