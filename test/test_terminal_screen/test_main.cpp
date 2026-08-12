#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "TerminalHistory.h"
#include "TerminalScreen.h"

void setUp() {}
void tearDown() {}

namespace {

constexpr size_t kLastRow = app::kTerminalRows - 1;
constexpr size_t kLastColumn = app::kTerminalColumns - 1;

app::TerminalCell styledCell(uint16_t glyph, uint8_t foreground, uint8_t background,
                             uint8_t attributes) {
    app::TerminalCell cell;
    cell.setGlyph(glyph);
    cell.setForeground(foreground);
    cell.setBackground(background);
    cell.setAttributes(attributes);
    return cell;
}

void fillRow(app::TerminalScreen& screen, uint8_t row, uint16_t glyph) {
    screen.setCursor(row, 0);
    app::TerminalRow removed{};
    for (size_t column = 0; column < app::kTerminalColumns; ++column) {
        screen.putGlyph(glyph, false, false, removed);
    }
}

void assertGlyphWithStyle(const app::TerminalCell& cell, uint16_t glyph,
                          uint8_t foreground, uint8_t background, uint8_t attributes) {
    TEST_ASSERT_EQUAL_HEX16(glyph, cell.glyph());
    TEST_ASSERT_EQUAL_UINT8(foreground, cell.foreground());
    TEST_ASSERT_EQUAL_UINT8(background, cell.background());
    TEST_ASSERT_EQUAL_UINT8(attributes, cell.attributes());
}

void assertBlankWithStyle(const app::TerminalCell& cell, uint8_t foreground,
                          uint8_t background, uint8_t attributes) {
    TEST_ASSERT_EQUAL_HEX16(' ', cell.glyph());
    TEST_ASSERT_EQUAL_UINT8(foreground, cell.foreground());
    TEST_ASSERT_EQUAL_UINT8(background, cell.background());
    TEST_ASSERT_EQUAL_UINT8(attributes, cell.attributes());
}

void testScreenResetAndEditsAreBounded() {
    app::TerminalScreen screen;
    TEST_ASSERT_EQUAL_UINT8(0, screen.cursor().row);
    TEST_ASSERT_EQUAL_UINT8(0, screen.cursor().column);
    TEST_ASSERT_TRUE(screen.cursor().visible);
    TEST_ASSERT_EQUAL_HEX16(' ', screen.cell(0, 0).glyph());

    screen.setCursor(kLastRow, kLastColumn);
    app::TerminalRow removed{};
    TEST_ASSERT_FALSE(screen.putGlyph('X', false, true, removed));
    TEST_ASSERT_EQUAL_HEX16('X', screen.cell(kLastRow, kLastColumn).glyph());
    screen.moveCursor(50, 50);
    TEST_ASSERT_EQUAL_UINT8(kLastRow, screen.cursor().row);
    TEST_ASSERT_EQUAL_UINT8(kLastColumn, screen.cursor().column);
    screen.moveCursor(-100, -100);
    TEST_ASSERT_EQUAL_UINT8(0, screen.cursor().row);
    TEST_ASSERT_EQUAL_UINT8(0, screen.cursor().column);

    screen.setCursor(kLastRow, kLastColumn);
    screen.eraseLine(app::TerminalEraseMode::All);
    TEST_ASSERT_EQUAL_HEX16(' ', screen.cell(kLastRow, kLastColumn).glyph());

    const app::TerminalCell& invalid = screen.cell(app::kTerminalRows, app::kTerminalColumns);
    TEST_ASSERT_EQUAL_HEX16(' ', invalid.glyph());
}

void testPutGlyphUsesVtWrapPendingSemantics() {
    app::TerminalScreen screen;
    app::TerminalRow removed{};

    screen.setCursor(0, kLastColumn);
    TEST_ASSERT_FALSE(screen.putGlyph('A', false, true, removed));
    TEST_ASSERT_EQUAL_UINT8(0, screen.cursor().row);
    TEST_ASSERT_EQUAL_UINT8(kLastColumn, screen.cursor().column);
    TEST_ASSERT_FALSE(screen.putGlyph('B', false, true, removed));
    TEST_ASSERT_EQUAL_HEX16('A', screen.cell(0, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_HEX16('B', screen.cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(1, screen.cursor().row);
    TEST_ASSERT_EQUAL_UINT8(1, screen.cursor().column);

    screen.setCursor(2, kLastColumn);
    screen.putGlyph('C', false, true, removed);
    screen.carriageReturn();
    screen.putGlyph('D', false, true, removed);
    TEST_ASSERT_EQUAL_HEX16('D', screen.cell(2, 0).glyph());

    screen.setCursor(3, kLastColumn);
    screen.putGlyph('E', false, false, removed);
    screen.putGlyph('F', false, false, removed);
    TEST_ASSERT_EQUAL_HEX16('F', screen.cell(3, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', screen.cell(4, 0).glyph());
}

void testFullScreenWrapAndLineFeedExtractRemovedRows() {
    app::TerminalScreen screen;
    screen.setStyle(2, 4, app::kTerminalBold);
    fillRow(screen, 0, 'Q');
    screen.setStyle(7, 0, 0);
    screen.setCursor(kLastRow, kLastColumn);
    app::TerminalRow removed{};
    screen.putGlyph('X', false, true, removed);
    TEST_ASSERT_TRUE(screen.putGlyph('Y', false, true, removed));
    assertGlyphWithStyle(removed[0], 'Q', 2, 4, app::kTerminalBold);
    assertGlyphWithStyle(removed[17], 'Q', 2, 4, app::kTerminalBold);
    assertGlyphWithStyle(removed[kLastColumn], 'Q', 2, 4, app::kTerminalBold);
    TEST_ASSERT_EQUAL_HEX16(
        'X', screen.cell(kLastRow - 1, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_HEX16('Y', screen.cell(kLastRow, 0).glyph());

    screen.setStyle(5, 6, app::kTerminalUnderline);
    fillRow(screen, 0, 'R');
    screen.setStyle(7, 0, 0);
    screen.setCursor(kLastRow, 3);
    TEST_ASSERT_TRUE(screen.lineFeed(removed));
    assertGlyphWithStyle(removed[0], 'R', 5, 6, app::kTerminalUnderline);
    assertGlyphWithStyle(removed[22], 'R', 5, 6, app::kTerminalUnderline);
    assertGlyphWithStyle(removed[kLastColumn], 'R', 5, 6,
                         app::kTerminalUnderline);
    TEST_ASSERT_EQUAL_UINT8(kLastRow, screen.cursor().row);
    TEST_ASSERT_EQUAL_UINT8(3, screen.cursor().column);
}

void testEraseAndCharacterEditingUseCurrentStyleForBlanks() {
    app::TerminalScreen screen;
    app::TerminalRow removed{};
    for (char glyph = 'A'; glyph <= 'E'; ++glyph) screen.putGlyph(glyph, false, true, removed);

    screen.setStyle(196, 17, app::kTerminalBold | app::kTerminalUnderline);
    screen.setCursor(0, 1);
    screen.insertCharacters(2);
    TEST_ASSERT_EQUAL_HEX16('A', screen.cell(0, 0).glyph());
    assertBlankWithStyle(screen.cell(0, 1), 196, 17,
                         app::kTerminalBold | app::kTerminalUnderline);
    assertBlankWithStyle(screen.cell(0, 2), 196, 17,
                         app::kTerminalBold | app::kTerminalUnderline);
    TEST_ASSERT_EQUAL_HEX16('B', screen.cell(0, 3).glyph());

    screen.deleteCharacters(2);
    TEST_ASSERT_EQUAL_HEX16('B', screen.cell(0, 1).glyph());
    assertBlankWithStyle(screen.cell(0, kLastColumn - 1), 196, 17,
                         app::kTerminalBold | app::kTerminalUnderline);
    assertBlankWithStyle(screen.cell(0, kLastColumn), 196, 17,
                         app::kTerminalBold | app::kTerminalUnderline);

    screen.setCursor(0, 2);
    screen.eraseLine(app::TerminalEraseMode::After);
    TEST_ASSERT_EQUAL_HEX16('B', screen.cell(0, 1).glyph());
    assertBlankWithStyle(screen.cell(0, 2), 196, 17,
                         app::kTerminalBold | app::kTerminalUnderline);

    screen.setCursor(1, 0);
    screen.putGlyph('Z', false, true, removed);
    screen.setCursor(1, 0);
    screen.eraseDisplay(app::TerminalEraseMode::Before);
    assertBlankWithStyle(screen.cell(0, 0), 196, 17,
                         app::kTerminalBold | app::kTerminalUnderline);
    assertBlankWithStyle(screen.cell(1, 0), 196, 17,
                         app::kTerminalBold | app::kTerminalUnderline);
}

void testInsertAndDeleteLinesStayInsidePartialMargins() {
    app::TerminalScreen screen;
    for (uint8_t row = 0; row < app::kTerminalRows; ++row) {
        fillRow(screen, row, static_cast<uint16_t>('A' + row));
    }
    screen.setStyle(46, 21, app::kTerminalInverse);
    screen.setMargins(3, 10);
    screen.setCursor(5, 0);
    screen.insertLines(2);
    TEST_ASSERT_EQUAL_HEX16('E', screen.cell(4, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('E', screen.cell(4, 19).glyph());
    assertBlankWithStyle(screen.cell(5, 17), 46, 21, app::kTerminalInverse);
    assertBlankWithStyle(screen.cell(6, kLastColumn), 46, 21,
                         app::kTerminalInverse);
    assertGlyphWithStyle(screen.cell(7, 23), 'F', 7, 0, 0);
    TEST_ASSERT_EQUAL_HEX16('L', screen.cell(11, 31).glyph());

    screen.deleteLines(0);
    assertBlankWithStyle(screen.cell(10, 0), 46, 21, app::kTerminalInverse);
    assertBlankWithStyle(screen.cell(10, 28), 46, 21, app::kTerminalInverse);
    assertGlyphWithStyle(screen.cell(6, 23), 'F', 7, 0, 0);
    TEST_ASSERT_EQUAL_HEX16('L', screen.cell(11, 31).glyph());

    screen.setCursor(2, 0);
    const uint16_t before = screen.cell(2, 0).glyph();
    screen.insertLines(1);
    screen.deleteLines(1);
    TEST_ASSERT_EQUAL_HEX16(before, screen.cell(2, 0).glyph());
}

void testPartialMarginScrollIsClippedAndNeverExtracted() {
    app::TerminalScreen screen;
    for (uint8_t row = 0; row < app::kTerminalRows; ++row) {
        fillRow(screen, row, static_cast<uint16_t>('A' + row));
    }
    screen.setStyle(3, 4, app::kTerminalBold);
    screen.setMargins(4, 8);
    app::TerminalRow removed{};
    removed[0] = styledCell('?', 1, 2, 0);
    TEST_ASSERT_FALSE(screen.scrollUp(2, &removed));
    TEST_ASSERT_EQUAL_HEX16('?', removed[0].glyph());
    TEST_ASSERT_EQUAL_HEX16('A', screen.cell(0, 0).glyph());
    assertGlyphWithStyle(screen.cell(4, 17), 'G', 7, 0, 0);
    assertGlyphWithStyle(screen.cell(5, 23), 'H', 7, 0, 0);
    assertBlankWithStyle(screen.cell(7, 17), 3, 4, app::kTerminalBold);
    assertBlankWithStyle(screen.cell(8, kLastColumn), 3, 4,
                         app::kTerminalBold);
    TEST_ASSERT_EQUAL_HEX16('J', screen.cell(9, 31).glyph());

    screen.scrollDown(1);
    assertBlankWithStyle(screen.cell(4, 17), 3, 4, app::kTerminalBold);
    assertGlyphWithStyle(screen.cell(5, kLastColumn), 'G', 7, 0, 0);
    assertGlyphWithStyle(screen.cell(6, 23), 'H', 7, 0, 0);
    TEST_ASSERT_EQUAL_HEX16('J', screen.cell(9, 31).glyph());
}

void testInvalidMarginsPreserveThePreviousScrollRegion() {
    app::TerminalScreen screen;
    for (uint8_t row = 0; row < app::kTerminalRows; ++row) {
        fillRow(screen, row, static_cast<uint16_t>('A' + row));
    }
    screen.setMargins(4, 8);
    screen.setMargins(9, 3);
    screen.setMargins(static_cast<uint8_t>(app::kTerminalRows),
                      static_cast<uint8_t>(app::kTerminalRows - 1));
    screen.setStyle(9, 10, app::kTerminalInverse);
    screen.setCursor(8, 17);

    app::TerminalRow removed{};
    removed[0] = styledCell('?', 1, 2, 0);
    TEST_ASSERT_FALSE(screen.lineFeed(removed));
    TEST_ASSERT_EQUAL_HEX16('?', removed[0].glyph());
    assertGlyphWithStyle(screen.cell(3, 17), 'D', 7, 0, 0);
    assertGlyphWithStyle(screen.cell(4, 17), 'F', 7, 0, 0);
    assertGlyphWithStyle(screen.cell(7, 17), 'I', 7, 0, 0);
    assertBlankWithStyle(screen.cell(8, 17), 9, 10, app::kTerminalInverse);
    assertGlyphWithStyle(screen.cell(9, 17), 'J', 7, 0, 0);
}

void testMarginAccessorsExposeOnlyValidatedScreenState() {
    app::TerminalScreen screen;
    TEST_ASSERT_EQUAL_UINT8(0, screen.marginTop());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalRows - 1, screen.marginBottom());

    screen.setMargins(4, 8);
    TEST_ASSERT_EQUAL_UINT8(4, screen.marginTop());
    TEST_ASSERT_EQUAL_UINT8(8, screen.marginBottom());

    screen.setMargins(9, 3);
    screen.setMargins(static_cast<uint8_t>(app::kTerminalRows),
                      static_cast<uint8_t>(app::kTerminalRows - 1));
    TEST_ASSERT_EQUAL_UINT8(4, screen.marginTop());
    TEST_ASSERT_EQUAL_UINT8(8, screen.marginBottom());

    screen.reset();
    TEST_ASSERT_EQUAL_UINT8(0, screen.marginTop());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalRows - 1, screen.marginBottom());
}

void testReverseIndexUsesMarginTopAndLeavesPhysicalTopOutsideItAlone() {
    app::TerminalScreen screen;
    for (uint8_t row = 0; row < app::kTerminalRows; ++row) {
        fillRow(screen, row, static_cast<uint16_t>('A' + row));
    }
    screen.setMargins(4, 8);
    screen.setCursor(4, 17);
    screen.clearDirtyRows();

    TEST_ASSERT_TRUE(screen.reverseIndex());
    TEST_ASSERT_EQUAL_UINT8(4, screen.cursor().row);
    TEST_ASSERT_EQUAL_UINT8(17, screen.cursor().column);
    TEST_ASSERT_EQUAL_HEX16('D', screen.cell(3, 17).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', screen.cell(4, 17).glyph());
    TEST_ASSERT_EQUAL_HEX16('E', screen.cell(5, 17).glyph());
    TEST_ASSERT_EQUAL_HEX16('H', screen.cell(8, 17).glyph());
    TEST_ASSERT_EQUAL_HEX16('J', screen.cell(9, 17).glyph());
    TEST_ASSERT_EQUAL_HEX16(0x01f0, screen.dirtyRows());

    screen.setCursor(0, kLastColumn);
    app::TerminalRow removed{};
    screen.putGlyph('Z', false, true, removed);
    screen.clearDirtyRows();
    TEST_ASSERT_FALSE(screen.reverseIndex());
    TEST_ASSERT_EQUAL_UINT8(0, screen.cursor().row);
    TEST_ASSERT_EQUAL_UINT8(kLastColumn, screen.cursor().column);
    TEST_ASSERT_EQUAL_HEX16(0, screen.dirtyRows());
    TEST_ASSERT_FALSE(screen.putGlyph('Y', false, true, removed));
    TEST_ASSERT_EQUAL_HEX16('Y', screen.cell(0, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_HEX16('B', screen.cell(1, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', screen.cell(4, 17).glyph());
}

void testDirtyRowsUseExactlyVisibleRowBits() {
    app::TerminalScreen screen;
    const uint16_t visibleMask =
        static_cast<uint16_t>((1U << app::kTerminalRows) - 1U);
    TEST_ASSERT_EQUAL_HEX16(visibleMask, screen.dirtyRows());
    screen.clearDirtyRows();
    TEST_ASSERT_EQUAL_HEX16(0, screen.dirtyRows());

    app::TerminalRow removed{};
    screen.setCursor(kLastRow, kLastColumn);
    TEST_ASSERT_EQUAL_HEX16((1U << 0) | (1U << kLastRow),
                            screen.dirtyRows());
    screen.clearDirtyRows();
    screen.putGlyph('X', false, false, removed);
    TEST_ASSERT_EQUAL_HEX16(1U << kLastRow, screen.dirtyRows());
    screen.clearDirtyRows();
    screen.setCursorVisible(false);
    TEST_ASSERT_EQUAL_HEX16(1U << kLastRow, screen.dirtyRows());

    screen.clearDirtyRows();
    screen.setMargins(4, 8);
    screen.scrollUp(1, nullptr);
    TEST_ASSERT_EQUAL_HEX16(0x01f0, screen.dirtyRows());
    TEST_ASSERT_EQUAL_HEX16(0, screen.dirtyRows() & 0x8000);
}

void testHistoryEvictsOldestRowAndTracksGeneration() {
    app::TerminalHistory history;
    app::TerminalRow row{};
    TEST_ASSERT_EQUAL_UINT(0, history.size());
    TEST_ASSERT_EQUAL_UINT32(0, history.generation());

    for (uint16_t index = 0; index <= app::kTerminalHistoryRows; ++index) {
        row[0].setGlyph(static_cast<uint16_t>('A' + index % 26));
        history.push(row);
    }
    TEST_ASSERT_EQUAL_UINT(app::kTerminalHistoryRows, history.size());
    TEST_ASSERT_EQUAL_UINT32(app::kTerminalHistoryRows + 1, history.generation());
    TEST_ASSERT_EQUAL_HEX16('B', history.row(0)[0].glyph());
    TEST_ASSERT_EQUAL_HEX16('Q', history.row(app::kTerminalHistoryRows - 1)[0].glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', history.row(app::kTerminalHistoryRows)[0].glyph());

    const uint32_t generationBeforeClear = history.generation();
    history.clear();
    TEST_ASSERT_EQUAL_UINT(0, history.size());
    TEST_ASSERT_EQUAL_UINT32(generationBeforeClear + 1, history.generation());
    TEST_ASSERT_EQUAL_HEX16(' ', history.row(0)[0].glyph());

    history.push(row);
    TEST_ASSERT_EQUAL_UINT32(generationBeforeClear + 2, history.generation());
    history.clear();
    TEST_ASSERT_EQUAL_UINT32(generationBeforeClear + 3, history.generation());
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testScreenResetAndEditsAreBounded);
    RUN_TEST(testPutGlyphUsesVtWrapPendingSemantics);
    RUN_TEST(testFullScreenWrapAndLineFeedExtractRemovedRows);
    RUN_TEST(testEraseAndCharacterEditingUseCurrentStyleForBlanks);
    RUN_TEST(testInsertAndDeleteLinesStayInsidePartialMargins);
    RUN_TEST(testPartialMarginScrollIsClippedAndNeverExtracted);
    RUN_TEST(testInvalidMarginsPreserveThePreviousScrollRegion);
    RUN_TEST(testMarginAccessorsExposeOnlyValidatedScreenState);
    RUN_TEST(testReverseIndexUsesMarginTopAndLeavesPhysicalTopOutsideItAlone);
    RUN_TEST(testDirtyRowsUseExactlyVisibleRowBits);
    RUN_TEST(testHistoryEvictsOldestRowAndTracksGeneration);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runTests();
}
void loop() {}
#else
int main(int, char**) {
    return runTests();
}
#endif
