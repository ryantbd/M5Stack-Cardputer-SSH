#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <stddef.h>
#include <stdint.h>

#include "TerminalEmulator.h"
#include "TerminalPalette.h"

void setUp() {}
void tearDown() {}

namespace {

app::TerminalEmulator terminal;
constexpr size_t kLastRow = app::kTerminalRows - 1;
constexpr size_t kLastColumn = app::kTerminalColumns - 1;
constexpr uint16_t kVisibleRowsMask =
    static_cast<uint16_t>((1U << app::kTerminalRows) - 1U);

void feedText(app::TerminalEmulator& terminal, const char* text) {
    size_t length = 0;
    while (text[length] != '\0') ++length;
    terminal.feed(reinterpret_cast<const uint8_t*>(text), length);
}

void seedRows(size_t count) {
    for (size_t row = 0; row < count; ++row) {
        const uint8_t glyph = static_cast<uint8_t>('A' + row);
        terminal.feed(&glyph, 1);
        if (row + 1 < count) {
            const uint8_t nextRow[] = {'\r', '\n'};
            terminal.feed(nextRow, sizeof(nextRow));
        }
    }
}

void assertCursor(uint8_t row, uint8_t column) {
    TEST_ASSERT_EQUAL_UINT8(row, terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(column, terminal.activeScreen().cursor().column);
}

void assertMalformedExtendedSgrPreservesStyle(const char* sequence,
                                              uint8_t initialAttributes,
                                              bool byteAtATime) {
    terminal.reset();
    feedText(terminal, "\x1b[33;45m");
    if ((initialAttributes & app::kTerminalBold) != 0) feedText(terminal, "\x1b[1m");
    if ((initialAttributes & app::kTerminalUnderline) != 0) {
        feedText(terminal, "\x1b[4m");
    }
    if ((initialAttributes & app::kTerminalInverse) != 0) feedText(terminal, "\x1b[7m");

    if (byteAtATime) {
        for (size_t index = 0; sequence[index] != '\0'; ++index) {
            terminal.feed(reinterpret_cast<const uint8_t*>(&sequence[index]), 1);
        }
    } else {
        feedText(terminal, sequence);
    }
    feedText(terminal, "X");
    TEST_ASSERT_EQUAL_UINT8(3, terminal.activeScreen().cell(0, 0).foreground());
    TEST_ASSERT_EQUAL_UINT8(5, terminal.activeScreen().cell(0, 0).background());
    TEST_ASSERT_EQUAL_UINT8(initialAttributes,
                            terminal.activeScreen().cell(0, 0).attributes());

    const char recovery[] = "\x1b[32mY";
    if (byteAtATime) {
        for (size_t index = 0; recovery[index] != '\0'; ++index) {
            terminal.feed(reinterpret_cast<const uint8_t*>(&recovery[index]), 1);
        }
    } else {
        feedText(terminal, recovery);
    }
    TEST_ASSERT_EQUAL_UINT8(2, terminal.activeScreen().cell(0, 1).foreground());
    TEST_ASSERT_EQUAL_UINT8(5, terminal.activeScreen().cell(0, 1).background());
    TEST_ASSERT_EQUAL_UINT8(initialAttributes,
                            terminal.activeScreen().cell(0, 1).attributes());
}

void testBasicTextControlsAndChunking() {
    terminal.reset();
    const uint8_t first[] = {'a', 'b', 'c', '\r', 'X', '\n'};
    terminal.feed(first, sizeof(first));
    TEST_ASSERT_EQUAL_HEX16('X', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('b', terminal.activeScreen().cell(0, 1).glyph());
    TEST_ASSERT_EQUAL_UINT8(1, terminal.activeScreen().cursor().row);

    const uint8_t arrowPart1[] = {0xe2, 0x86};
    const uint8_t arrowPart2[] = {0x92};
    terminal.feed(arrowPart1, sizeof(arrowPart1));
    terminal.feed(arrowPart2, sizeof(arrowPart2));
    TEST_ASSERT_EQUAL_HEX16(0x2192, terminal.activeScreen().cell(1, 1).glyph());
    TEST_ASSERT_EQUAL_UINT8(2, terminal.activeScreen().cursor().column);
}

void testEveryByteCanArriveInItsOwnChunk() {
    terminal.reset();
    const uint8_t stream[] = {
        'x', 0x1b, '7', '\r', '\n', 0xe2, 0x86, 0x92, 0x1b, '8', 'y',
    };
    for (size_t index = 0; index < sizeof(stream); ++index) {
        terminal.feed(&stream[index], 1);
    }

    TEST_ASSERT_EQUAL_HEX16('x', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('y', terminal.activeScreen().cell(0, 1).glyph());
    TEST_ASSERT_EQUAL_HEX16(0x2192, terminal.activeScreen().cell(1, 0).glyph());
}

void testTabBackspaceAndLineControlsStayBounded() {
    terminal.reset();
    feedText(terminal, "A\tB\bX");
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('X', terminal.activeScreen().cell(0, 8).glyph());
    TEST_ASSERT_EQUAL_UINT8(9, terminal.activeScreen().cursor().column);

    terminal.reset();
    const uint8_t controls[] = {'\b', 'Z', '\r', '\n', '\v', '\f'};
    terminal.feed(controls, sizeof(controls));
    TEST_ASSERT_EQUAL_HEX16('Z', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(3, terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cursor().column);
}

void testTabAtLastColumnCancelsPendingWrap() {
    terminal.reset();
    uint8_t line[app::kTerminalColumns];
    for (size_t column = 0; column < app::kTerminalColumns; ++column) line[column] = 'A';
    terminal.feed(line, sizeof(line));

    const uint32_t beforeTab = terminal.generation();
    const uint8_t tab = '\t';
    terminal.feed(&tab, 1);
    TEST_ASSERT_EQUAL_UINT32(beforeTab, terminal.generation());

    const uint8_t glyph = 'B';
    terminal.feed(&glyph, 1);
    TEST_ASSERT_EQUAL_HEX16(
        'B', terminal.activeScreen().cell(0, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(kLastColumn,
                            terminal.activeScreen().cursor().column);
}

void testMalformedAndUnsupportedUtf8UseReplacement() {
    terminal.reset();
    const uint8_t stream[] = {
        0xe2, 'A',              // truncated sequence, then a reprocessed ASCII lead
        0x80,                   // stray continuation
        0xe0, 0x80, 0x80,      // overlong three-byte form
        0xed, 0xa0, 0x80,      // surrogate
        0xf4, 0x90, 0x80, 0x80,// above U+10FFFF
        0xe4, 0xb8, 0xad,      // valid but unsupported CJK
        0xc3, 0xa9,            // supported Latin-1 e acute
    };
    terminal.feed(stream, sizeof(stream));

    TEST_ASSERT_EQUAL_HEX16(app::kTerminalReplacementGlyph,
                            terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 1).glyph());
    for (size_t column = 2; column <= 6; ++column) {
        TEST_ASSERT_EQUAL_HEX16(app::kTerminalReplacementGlyph,
                                terminal.activeScreen().cell(0, column).glyph());
    }
    TEST_ASSERT_EQUAL_HEX16(0x00e9, terminal.activeScreen().cell(0, 7).glyph());
}

void testInvalidContinuationReprocessesEscapeAndControlBytes() {
    terminal.reset();
    const uint8_t stream[] = {0xe2, 0x1b, '7', 'A', 0xe2, '\r', 'B'};
    terminal.feed(stream, sizeof(stream));
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 1).glyph());
    TEST_ASSERT_EQUAL_HEX16(app::kTerminalReplacementGlyph,
                            terminal.activeScreen().cell(0, 2).glyph());
}

void testExactConfiguredColumnWrap() {
    terminal.reset();
    uint8_t line[app::kTerminalColumns];
    for (size_t column = 0; column < app::kTerminalColumns; ++column) line[column] = 'A';
    terminal.feed(line, sizeof(line));

    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalColumns - 1,
                            terminal.activeScreen().cursor().column);
    TEST_ASSERT_EQUAL_HEX16(
        'A', terminal.activeScreen().cell(0, kLastColumn).glyph());

    const uint8_t next = 'B';
    terminal.feed(&next, 1);
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(1, terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(1, terminal.activeScreen().cursor().column);
}

void testFullScreenLineFeedEntersPrimaryHistory() {
    terminal.reset();
    for (size_t row = 0; row < app::kTerminalRows; ++row) {
        const uint8_t line[] = {static_cast<uint8_t>('A' + row), '\r', '\n'};
        terminal.feed(line, sizeof(line));
    }

    TEST_ASSERT_EQUAL_UINT(1, terminal.history().size());
    TEST_ASSERT_EQUAL_HEX16('A', terminal.history().row(0)[0].glyph());
    TEST_ASSERT_EQUAL_HEX16('B', terminal.primaryScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalRows - 1,
                            terminal.activeScreen().cursor().row);
}

void testEscIndexNextLineReverseIndexAndCursorSaveRestore() {
    terminal.reset();
    feedText(terminal, "A\x1b" "7\x1b" "EBC\x1b" "8D");
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('D', terminal.activeScreen().cell(0, 1).glyph());
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('C', terminal.activeScreen().cell(1, 1).glyph());

    terminal.reset();
    feedText(terminal, "A\x1b" "D\x1b" "MZ");
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('Z', terminal.activeScreen().cell(0, 1).glyph());

    terminal.reset();
    feedText(terminal, "A\x1b" "M");
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT(0, terminal.history().size());
}

void testCursorRestoreCancelsPendingWrapWithoutVisibleGenerationChange() {
    terminal.reset();
    uint8_t prefix[app::kTerminalColumns - 1];
    for (size_t column = 0; column < sizeof(prefix); ++column) prefix[column] = 'A';
    terminal.feed(prefix, sizeof(prefix));
    feedText(terminal, "\x1b" "7B");

    const uint32_t beforeRestore = terminal.generation();
    feedText(terminal, "\x1b" "8");
    TEST_ASSERT_EQUAL_UINT32(beforeRestore, terminal.generation());

    const uint8_t glyph = 'C';
    terminal.feed(&glyph, 1);
    TEST_ASSERT_EQUAL_HEX16(
        'C', terminal.activeScreen().cell(0, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cursor().row);
}

void testOscAndDcsCancelControlsWinAfterEscapeAcrossChunks() {
    terminal.reset();
    const uint8_t oscCanPrefix[] = {0x1b, ']', 'x', 0x1b};
    const uint8_t oscCanTail[] = {0x18, 'A'};
    terminal.feed(oscCanPrefix, sizeof(oscCanPrefix));
    terminal.feed(oscCanTail, sizeof(oscCanTail));

    const uint8_t oscSubPrefix[] = {0x1b, ']', 'x', 0x1b};
    const uint8_t oscSubTail[] = {0x1a, 'B'};
    terminal.feed(oscSubPrefix, sizeof(oscSubPrefix));
    terminal.feed(oscSubTail, sizeof(oscSubTail));

    const uint8_t oscBelPrefix[] = {0x1b, ']', 'x', 0x1b};
    const uint8_t oscBelTail[] = {0x07, 'C'};
    terminal.feed(oscBelPrefix, sizeof(oscBelPrefix));
    terminal.feed(oscBelTail, sizeof(oscBelTail));

    const uint8_t dcsCanPrefix[] = {0x1b, 'P', 'x', 0x1b};
    const uint8_t dcsCanTail[] = {0x18, 'D'};
    terminal.feed(dcsCanPrefix, sizeof(dcsCanPrefix));
    terminal.feed(dcsCanTail, sizeof(dcsCanTail));

    const uint8_t dcsSubPrefix[] = {0x1b, 'P', 'x', 0x1b};
    const uint8_t dcsSubTail[] = {0x1a, 'E'};
    terminal.feed(dcsSubPrefix, sizeof(dcsSubPrefix));
    terminal.feed(dcsSubTail, sizeof(dcsSubTail));

    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(0, 1).glyph());
    TEST_ASSERT_EQUAL_HEX16('C', terminal.activeScreen().cell(0, 2).glyph());
    TEST_ASSERT_EQUAL_HEX16('D', terminal.activeScreen().cell(0, 3).glyph());
    TEST_ASSERT_EQUAL_HEX16('E', terminal.activeScreen().cell(0, 4).glyph());
}

void testResetAndDiscardStatesNeverPrintSequencePayloads() {
    terminal.reset();
    const uint8_t first[] = {
        'a', 0x1b, '[', '1', '2', '3', ';', '4', 'z',
        0x1b, ']', 't', 'i', 't', 'l', 'e', 0x07,
        0x1b, 'P', 'd', 'a', 't', 'a', 0x1b, '\\',
        0x1b, '(', 'B',
        'b',
    };
    terminal.feed(first, sizeof(first));
    TEST_ASSERT_EQUAL_HEX16('a', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('b', terminal.activeScreen().cell(0, 1).glyph());

    const uint8_t oscPart1[] = {0x1b, ']', 'x', 0x1b};
    const uint8_t oscPart2[] = {'q', 0x1b, '\\', 'c'};
    terminal.feed(oscPart1, sizeof(oscPart1));
    terminal.feed(oscPart2, sizeof(oscPart2));
    TEST_ASSERT_EQUAL_HEX16('c', terminal.activeScreen().cell(0, 2).glyph());

    feedText(terminal, "dirty\x1b" "c");
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cursor().column);
    TEST_ASSERT_EQUAL_UINT(0, terminal.history().size());
}

void testAccessorsGenerationAndDirtyRowsTrackVisibleChanges() {
    terminal.reset();
    const uint32_t generationBaseline = terminal.generation();
    TEST_ASSERT_FALSE(terminal.alternateActive());
    TEST_ASSERT_FALSE(terminal.inputModes().applicationCursor);
    TEST_ASSERT_EQUAL_UINT32(generationBaseline, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(kVisibleRowsMask, terminal.dirtyRows());
    terminal.clearDirtyRows();
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());

    const uint8_t ignored[] = {0x07, 0x1b, '[', '9', '9', 'z'};
    terminal.feed(ignored, sizeof(ignored));
    TEST_ASSERT_EQUAL_UINT32(generationBaseline, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());

    const uint8_t glyph = 'A';
    terminal.feed(&glyph, 1);
    TEST_ASSERT_EQUAL_UINT32(generationBaseline + 1, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(1, terminal.dirtyRows());
    TEST_ASSERT_EQUAL_HEX16('A', terminal.primaryScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.alternateScreen().cell(0, 0).glyph());

    terminal.clearDirtyRows();
    const uint32_t beforeInvalidation = terminal.generation();
    terminal.invalidateAll();
    TEST_ASSERT_EQUAL_HEX16(kVisibleRowsMask, terminal.dirtyRows());
    TEST_ASSERT_EQUAL_UINT32(beforeInvalidation, terminal.generation());
}

void testCsiCursorMovementDefaultsCoordinatesAndClamping() {
    terminal.reset();
    feedText(terminal, "\x1b[5;6H");
    assertCursor(4, 5);
    feedText(terminal, "\x1b[2A");
    assertCursor(2, 5);
    feedText(terminal, "\x1b[3B");
    assertCursor(5, 5);
    feedText(terminal, "\x1b[4C");
    assertCursor(5, 9);
    feedText(terminal, "\x1b[2D");
    assertCursor(5, 7);
    feedText(terminal, "\x1b[2E");
    assertCursor(7, 0);
    feedText(terminal, "\x1b[3F");
    assertCursor(4, 0);
    feedText(terminal, "\x1b[12G");
    assertCursor(4, 11);
    feedText(terminal, "\x1b[9d");
    assertCursor(8, 11);
    feedText(terminal, "\x1b[;H");
    assertCursor(0, 0);
    feedText(terminal, "\x1b[13;40f");
    assertCursor(kLastRow, kLastColumn);

    feedText(terminal, "\x1b[0A\x1b[0D");
    assertCursor(kLastRow - 1, kLastColumn - 1);
    feedText(terminal, "\x1b[999B\x1b[999C");
    assertCursor(kLastRow, kLastColumn);
    feedText(terminal, "\x1b[0E\x1b[0F");
    assertCursor(kLastRow - 1, 0);
    feedText(terminal, "\x1b[0G\x1b[0d");
    assertCursor(0, 0);
}

void testCsiEraseLineModesUseCurrentStyle() {
    terminal.reset();
    feedText(terminal, "ABCDE\x1b[31;44;1m\x1b[1;3H\x1b[K");
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(0, 1).glyph());
    for (size_t column = 2; column < app::kTerminalColumns; ++column) {
        TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, column).glyph());
    }
    TEST_ASSERT_EQUAL_UINT8(1, terminal.activeScreen().cell(0, 2).foreground());
    TEST_ASSERT_EQUAL_UINT8(4, terminal.activeScreen().cell(0, 2).background());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalBold,
                            terminal.activeScreen().cell(0, 2).attributes());

    terminal.reset();
    feedText(terminal, "ABCDE\x1b[32;45;4m\x1b[1;3H\x1b[1K");
    for (size_t column = 0; column <= 2; ++column) {
        TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, column).glyph());
    }
    TEST_ASSERT_EQUAL_HEX16('D', terminal.activeScreen().cell(0, 3).glyph());
    TEST_ASSERT_EQUAL_UINT8(2, terminal.activeScreen().cell(0, 0).foreground());
    TEST_ASSERT_EQUAL_UINT8(5, terminal.activeScreen().cell(0, 0).background());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalUnderline,
                            terminal.activeScreen().cell(0, 0).attributes());

    terminal.reset();
    feedText(terminal, "ABCDE\x1b[1;3H\x1b[2K");
    for (size_t column = 0; column < app::kTerminalColumns; ++column) {
        TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, column).glyph());
    }
}

void testCsiEraseDisplayModesAndHistoryOnlyMode() {
    terminal.reset();
    feedText(terminal, "ABCDE\x1b[2;1HFGHIJ\x1b[1;3H\x1b[33;46;7m\x1b[J");
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(0, 1).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, 2).glyph());
    TEST_ASSERT_EQUAL_UINT8(3, terminal.activeScreen().cell(0, 2).foreground());
    TEST_ASSERT_EQUAL_UINT8(6, terminal.activeScreen().cell(0, 2).background());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalInverse,
                            terminal.activeScreen().cell(0, 2).attributes());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(1, 0).glyph());

    terminal.reset();
    feedText(terminal, "ABCDE\x1b[2;1HFGHIJ\x1b[2;3H\x1b[1J");
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, 4).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(1, 2).glyph());
    TEST_ASSERT_EQUAL_HEX16('I', terminal.activeScreen().cell(1, 3).glyph());

    terminal.reset();
    feedText(terminal, "ABCDE\x1b[2J");
    for (size_t row = 0; row < app::kTerminalRows; ++row) {
        for (size_t column = 0; column < app::kTerminalColumns; ++column) {
            TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(row, column).glyph());
        }
    }

    terminal.reset();
    seedRows(app::kTerminalRows);
    feedText(terminal, "\r\nP\r\nQ");
    const uint8_t scrollLine[] = {'\r', '\n', 'R'};
    while (terminal.history().size() < app::kTerminalHistoryRows) {
        terminal.feed(scrollLine, sizeof(scrollLine));
    }
    TEST_ASSERT_EQUAL_UINT(app::kTerminalHistoryRows, terminal.history().size());
    const uint16_t visibleTop = terminal.activeScreen().cell(0, 0).glyph();
    const uint16_t visibleBottom =
        terminal.activeScreen().cell(kLastRow, 0).glyph();
    feedText(terminal, "\x1b[3J");
    TEST_ASSERT_EQUAL_UINT(0, terminal.history().size());
    TEST_ASSERT_EQUAL_HEX16(visibleTop, terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16(visibleBottom,
                            terminal.activeScreen().cell(kLastRow, 0).glyph());
}

void testCsiInsertDeleteCharactersAndStylePreservingBlanks() {
    terminal.reset();
    feedText(terminal, "abcdef\x1b[1;3H\x1b[31;44;1;4m\x1b[2@");
    TEST_ASSERT_EQUAL_HEX16('a', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('b', terminal.activeScreen().cell(0, 1).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, 2).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, 3).glyph());
    TEST_ASSERT_EQUAL_HEX16('c', terminal.activeScreen().cell(0, 4).glyph());
    TEST_ASSERT_EQUAL_UINT8(1, terminal.activeScreen().cell(0, 2).foreground());
    TEST_ASSERT_EQUAL_UINT8(4, terminal.activeScreen().cell(0, 2).background());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalBold | app::kTerminalUnderline,
                            terminal.activeScreen().cell(0, 2).attributes());

    feedText(terminal, "\x1b[0P");
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, 2).glyph());
    TEST_ASSERT_EQUAL_HEX16('c', terminal.activeScreen().cell(0, 3).glyph());
    feedText(terminal, "\x1b[P");
    TEST_ASSERT_EQUAL_HEX16('c', terminal.activeScreen().cell(0, 2).glyph());

    feedText(terminal, "\x1b[99999@");
    for (size_t column = 2; column < app::kTerminalColumns; ++column) {
        TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, column).glyph());
    }
}

void testCsiInsertDeleteLinesRespectMarginsAndStyle() {
    terminal.reset();
    seedRows(6);
    feedText(terminal, "\x1b[2;5r\x1b[3;1H\x1b[32;45;4m\x1b[2L");
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(2, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(3, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('C', terminal.activeScreen().cell(4, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('F', terminal.activeScreen().cell(5, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(2, terminal.activeScreen().cell(2, 0).foreground());
    TEST_ASSERT_EQUAL_UINT8(5, terminal.activeScreen().cell(2, 0).background());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalUnderline,
                            terminal.activeScreen().cell(2, 0).attributes());

    terminal.reset();
    seedRows(6);
    feedText(terminal, "\x1b[2;5r\x1b[3;1H\x1b[31;44;1m\x1b[0M");
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('D', terminal.activeScreen().cell(2, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('E', terminal.activeScreen().cell(3, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(4, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('F', terminal.activeScreen().cell(5, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(1, terminal.activeScreen().cell(4, 0).foreground());
    TEST_ASSERT_EQUAL_UINT8(4, terminal.activeScreen().cell(4, 0).background());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalBold,
                            terminal.activeScreen().cell(4, 0).attributes());
}

void testCsiScrollPreservesEveryFullScreenHistoryRow() {
    terminal.reset();
    seedRows(app::kTerminalRows);
    feedText(terminal, "\x1b[33;44;1m\x1b[3S");
    TEST_ASSERT_EQUAL_UINT(3, terminal.history().size());
    TEST_ASSERT_EQUAL_HEX16('A', terminal.history().row(0)[0].glyph());
    TEST_ASSERT_EQUAL_HEX16('B', terminal.history().row(1)[0].glyph());
    TEST_ASSERT_EQUAL_HEX16('C', terminal.history().row(2)[0].glyph());
    TEST_ASSERT_EQUAL_HEX16('D', terminal.activeScreen().cell(0, 0).glyph());
    for (size_t row = app::kTerminalRows - 3; row < app::kTerminalRows;
         ++row) {
        TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(row, 0).glyph());
        TEST_ASSERT_EQUAL_UINT8(3, terminal.activeScreen().cell(row, 0).foreground());
        TEST_ASSERT_EQUAL_UINT8(4, terminal.activeScreen().cell(row, 0).background());
        TEST_ASSERT_EQUAL_UINT8(app::kTerminalBold,
                                terminal.activeScreen().cell(row, 0).attributes());
    }

    feedText(terminal, "\x1b[2T");
    TEST_ASSERT_EQUAL_UINT(3, terminal.history().size());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('D', terminal.activeScreen().cell(2, 0).glyph());
}

void testCsiPartialScrollAndMarginValidation() {
    terminal.reset();
    seedRows(6);
    feedText(terminal, "\x1b[2;4r");
    assertCursor(0, 0);
    feedText(terminal, "\x1b[3;2H\x1b[5;3r");
    assertCursor(2, 1);
    feedText(terminal, "\x1b[2S");
    TEST_ASSERT_EQUAL_UINT(0, terminal.history().size());
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('D', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(2, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(3, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('E', terminal.activeScreen().cell(4, 0).glyph());

    feedText(terminal, "\x1b[0T");
    TEST_ASSERT_EQUAL_UINT(0, terminal.history().size());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(1, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('D', terminal.activeScreen().cell(2, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('E', terminal.activeScreen().cell(4, 0).glyph());

    feedText(terminal, "\x1b[0;999r");
    assertCursor(0, 0);
    feedText(terminal, "\x1b[999;1r");
    assertCursor(0, 0);
    feedText(terminal, "\x1b[13;1H\n");
    TEST_ASSERT_EQUAL_UINT(1, terminal.history().size());

    terminal.reset();
    feedText(terminal, "\x1b[3;7r\x1b[r");
    assertCursor(0, 0);
    seedRows(app::kTerminalRows);
    feedText(terminal, "\r\n");
    TEST_ASSERT_EQUAL_UINT(1, terminal.history().size());
}

void testSgrAttributesAndAllColorForms() {
    terminal.reset();
    feedText(terminal,
             "A"
             "\x1b[1;4;7mB"
             "\x1b[22mC"
             "\x1b[24;27mD"
             "\x1b[31;44mE"
             "\x1b[39;49mF"
             "\x1b[91;104mG"
             "\x1b[38;5;196;48;5;17mH"
             "\x1b[38;2;12;200;90;48;2;250;10;130mI"
             "\x1b[0mJ");

    TEST_ASSERT_EQUAL_UINT8(7, terminal.activeScreen().cell(0, 0).foreground());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cell(0, 0).background());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cell(0, 0).attributes());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalBold | app::kTerminalUnderline |
                                app::kTerminalInverse,
                            terminal.activeScreen().cell(0, 1).attributes());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalUnderline | app::kTerminalInverse,
                            terminal.activeScreen().cell(0, 2).attributes());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cell(0, 3).attributes());
    TEST_ASSERT_EQUAL_UINT8(1, terminal.activeScreen().cell(0, 4).foreground());
    TEST_ASSERT_EQUAL_UINT8(4, terminal.activeScreen().cell(0, 4).background());
    TEST_ASSERT_EQUAL_UINT8(7, terminal.activeScreen().cell(0, 5).foreground());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cell(0, 5).background());
    TEST_ASSERT_EQUAL_UINT8(9, terminal.activeScreen().cell(0, 6).foreground());
    TEST_ASSERT_EQUAL_UINT8(12, terminal.activeScreen().cell(0, 6).background());
    TEST_ASSERT_EQUAL_UINT8(196, terminal.activeScreen().cell(0, 7).foreground());
    TEST_ASSERT_EQUAL_UINT8(17, terminal.activeScreen().cell(0, 7).background());
    TEST_ASSERT_EQUAL_UINT8(app::nearestXtermColor(12, 200, 90),
                            terminal.activeScreen().cell(0, 8).foreground());
    TEST_ASSERT_EQUAL_UINT8(app::nearestXtermColor(250, 10, 130),
                            terminal.activeScreen().cell(0, 8).background());
    TEST_ASSERT_EQUAL_UINT8(7, terminal.activeScreen().cell(0, 9).foreground());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cell(0, 9).background());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cell(0, 9).attributes());

    terminal.reset();
    feedText(terminal, "\x1b[38;5;999;48;2;999;0;0mX");
    TEST_ASSERT_EQUAL_UINT8(255, terminal.activeScreen().cell(0, 0).foreground());
    TEST_ASSERT_EQUAL_UINT8(app::nearestXtermColor(255, 0, 0),
                            terminal.activeScreen().cell(0, 0).background());
}

void testSgrEveryStandardAndBrightColor() {
    terminal.reset();
    feedText(terminal,
             "\x1b[30mA\x1b[31mB\x1b[32mC\x1b[33mD"
             "\x1b[34mE\x1b[35mF\x1b[36mG\x1b[37mH\x1b[0m"
             "\x1b[40mI\x1b[41mJ\x1b[42mK\x1b[43mL"
             "\x1b[44mM\x1b[45mN\x1b[46mO\x1b[47mP\x1b[0m"
             "\x1b[90mQ\x1b[91mR\x1b[92mS\x1b[93mT"
             "\x1b[94mU\x1b[95mV\x1b[96mW\x1b[97mX\x1b[0m"
             "\x1b[100mY\x1b[101mZ\x1b[102ma\x1b[103mb"
             "\x1b[104mc\x1b[105md\x1b[106me\x1b[107mf");

    for (size_t color = 0; color < 8; ++color) {
        TEST_ASSERT_EQUAL_UINT8(color,
                                terminal.activeScreen().cell(0, color).foreground());
        TEST_ASSERT_EQUAL_UINT8(color,
                                terminal.activeScreen().cell(0, 8 + color).background());
        TEST_ASSERT_EQUAL_UINT8(color + 8,
                                terminal.activeScreen().cell(0, 16 + color).foreground());
        TEST_ASSERT_EQUAL_UINT8(color + 8,
                                terminal.activeScreen().cell(0, 24 + color).background());
    }
}

void testMalformedExtendedSgrIsAtomicAndRecovers() {
    constexpr uint8_t allAttributes =
        app::kTerminalBold | app::kTerminalUnderline | app::kTerminalInverse;
    assertMalformedExtendedSgrPreservesStyle("\x1b[38m", allAttributes, false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[31;38m", allAttributes, false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[48m", allAttributes, false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[38;5m", allAttributes, false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[48;5m", allAttributes, false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[38;2;0m", allAttributes, false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[38;2;1;4m",
                                              app::kTerminalInverse, false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[48;2;22;24m", allAttributes,
                                              false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[48;2;27m", allAttributes, false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[38;3;22;24;27m", allAttributes,
                                              false);
    assertMalformedExtendedSgrPreservesStyle("\x1b[48;9;0;1;4m",
                                              app::kTerminalInverse, true);
}

void testCompleteExtendedSgrConsumesPayloadAndAllowsLaterCodes() {
    terminal.reset();
    feedText(terminal,
             "\x1b[4m"
             "\x1b[38;5;1;22;48;2;0;1;4;1;24mX");
    TEST_ASSERT_EQUAL_UINT8(1, terminal.activeScreen().cell(0, 0).foreground());
    TEST_ASSERT_EQUAL_UINT8(app::nearestXtermColor(0, 1, 4),
                            terminal.activeScreen().cell(0, 0).background());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalBold,
                            terminal.activeScreen().cell(0, 0).attributes());
}

void testDecSpecialGraphicsMapsEveryRequestedCharacter() {
    terminal.reset();
    feedText(terminal, "\x1b(0jklmnopqrstuvwx\x1b(Bq");
    const uint16_t expected[] = {
        0x2518, 0x2510, 0x250c, 0x2514, 0x253c,
        0x2500, 0x2500, 0x2500, 0x2500, 0x2500,
        0x251c, 0x2524, 0x2534, 0x252c, 0x2502,
    };
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        TEST_ASSERT_EQUAL_HEX16(expected[index],
                                terminal.activeScreen().cell(0, index).glyph());
    }
    TEST_ASSERT_EQUAL_HEX16('q', terminal.activeScreen().cell(0, 15).glyph());
}

void testCsiParserBoundsDiscardMalformedSequences() {
    terminal.reset();
    feedText(terminal, "\x1b[0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;31mA");
    TEST_ASSERT_EQUAL_UINT8(1, terminal.activeScreen().cell(0, 0).foreground());

    terminal.reset();
    feedText(terminal, "\x1b[0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;31mB");
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(7, terminal.activeScreen().cell(0, 0).foreground());

    terminal.reset();
    feedText(terminal, "\x1b[99999CZ");
    TEST_ASSERT_EQUAL_HEX16(
        'Z', terminal.activeScreen().cell(0, kLastColumn).glyph());
    terminal.reset();
    feedText(terminal, "\x1b[100000CY");
    TEST_ASSERT_EQUAL_HEX16('Y', terminal.activeScreen().cell(0, 0).glyph());

    terminal.reset();
    feedText(terminal,
             "\x1b[?2H"
             "\x1b[??2H"
             "\x1b[2?H"
             "\x1b[2$H"
             "\x1b[2$$H"
             "\x1b[123456ABC");
    TEST_ASSERT_EQUAL_HEX16('B', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('C', terminal.activeScreen().cell(0, 1).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(0, 2).glyph());
}

void testCsiChunkingAndC0InterruptionPreserveParserState() {
    terminal.reset();
    const uint8_t first[] = {'A', 0x1b, '[', '2'};
    const uint8_t second[] = {'\r', ';'};
    const uint8_t third[] = {'3', 'H', 'X'};
    terminal.feed(first, sizeof(first));
    terminal.feed(second, sizeof(second));
    terminal.feed(third, sizeof(third));
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('X', terminal.activeScreen().cell(1, 2).glyph());

    const uint8_t cancel[] = {0x1b, '[', '9', 0x18, 'Y'};
    terminal.feed(cancel, sizeof(cancel));
    TEST_ASSERT_EQUAL_HEX16('Y', terminal.activeScreen().cell(1, 3).glyph());
}

void testRisResetsStyleGraphicsParserAndMargins() {
    terminal.reset();
    seedRows(3);
    feedText(terminal, "\x1b[2;3r\x1b[31;44;1;4;7m\x1b(0\x1b[123\x1b" "cq");
    TEST_ASSERT_EQUAL_UINT(0, terminal.history().size());
    TEST_ASSERT_EQUAL_HEX16('q', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(7, terminal.activeScreen().cell(0, 0).foreground());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cell(0, 0).background());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cell(0, 0).attributes());

    seedRows(app::kTerminalRows);
    feedText(terminal, "\r\n");
    TEST_ASSERT_EQUAL_UINT(1, terminal.history().size());
}

void assertReply(const app::TerminalReply& reply, const uint8_t* expected,
                 size_t length) {
    TEST_ASSERT_EQUAL_UINT8(length, reply.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, reply.bytes, length);
}

void testPrivateModesApplyAllParametersAndDriveScreenBehavior() {
    terminal.reset();
    feedText(terminal, "\x1b[?1;7;25h");
    TEST_ASSERT_TRUE(terminal.inputModes().applicationCursor);
    TEST_ASSERT_TRUE(terminal.activeScreen().cursor().visible);

    feedText(terminal, "\x1b[?1;25l");
    TEST_ASSERT_FALSE(terminal.inputModes().applicationCursor);
    TEST_ASSERT_FALSE(terminal.primaryScreen().cursor().visible);
    TEST_ASSERT_FALSE(terminal.alternateScreen().cursor().visible);

    terminal.reset();
    feedText(terminal, "\x1b[?7l");
    uint8_t line[app::kTerminalColumns];
    for (size_t column = 0; column < sizeof(line); ++column) line[column] = 'A';
    terminal.feed(line, sizeof(line));
    feedText(terminal, "B");
    TEST_ASSERT_EQUAL_HEX16(
        'B', terminal.activeScreen().cell(0, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.activeScreen().cell(1, 0).glyph());

    feedText(terminal, "\x1b[?7hCD");
    TEST_ASSERT_EQUAL_HEX16(
        'C', terminal.activeScreen().cell(0, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_HEX16('D', terminal.activeScreen().cell(1, 0).glyph());

    terminal.reset();
    feedText(terminal, "ABC\x1b[H\x1b[999;4hX");
    TEST_ASSERT_EQUAL_HEX16('X', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 1).glyph());
    TEST_ASSERT_EQUAL_HEX16('C', terminal.activeScreen().cell(0, 3).glyph());
    feedText(terminal, "\x1b[4l\x1b[HY");
    TEST_ASSERT_EQUAL_HEX16('Y', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('A', terminal.activeScreen().cell(0, 1).glyph());

    feedText(terminal, "\x1b[1;40H\x1b[4hZ");
    TEST_ASSERT_EQUAL_HEX16(
        'Z', terminal.activeScreen().cell(0, kLastColumn).glyph());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(kLastColumn,
                            terminal.activeScreen().cursor().column);
}

void testScreenSelectionPreservesStateAndNoOpSwitchesStayQuiet() {
    terminal.reset();
    feedText(terminal, "shell");
    const app::TerminalCursor primaryCursor = terminal.primaryScreen().cursor();
    terminal.clearDirtyRows();
    const uint32_t beforeEnter = terminal.generation();

    feedText(terminal, "\x1b[?47h");
    TEST_ASSERT_TRUE(terminal.alternateActive());
    TEST_ASSERT_EQUAL_UINT32(beforeEnter + 1, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(kVisibleRowsMask, terminal.dirtyRows());
    feedText(terminal, "alt");
    terminal.clearDirtyRows();
    const uint32_t beforeRepeatedEnter = terminal.generation();
    feedText(terminal, "\x1b[?47h\x1b[?1047h");
    TEST_ASSERT_EQUAL_UINT32(beforeRepeatedEnter, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());
    TEST_ASSERT_EQUAL_HEX16('a', terminal.alternateScreen().cell(0, 0).glyph());

    feedText(terminal, "\x1b[?47l");
    TEST_ASSERT_FALSE(terminal.alternateActive());
    TEST_ASSERT_EQUAL_HEX16('s', terminal.activeScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(primaryCursor.row, terminal.primaryScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(primaryCursor.column,
                            terminal.primaryScreen().cursor().column);
    TEST_ASSERT_EQUAL_UINT32(beforeRepeatedEnter + 1, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(kVisibleRowsMask, terminal.dirtyRows());

    terminal.clearDirtyRows();
    const uint32_t beforeRepeatedLeave = terminal.generation();
    feedText(terminal, "\x1b[?47l\x1b[?1047l");
    TEST_ASSERT_EQUAL_UINT32(beforeRepeatedLeave, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());

    feedText(terminal, "\x1b[?1047h");
    TEST_ASSERT_EQUAL_HEX16('a', terminal.activeScreen().cell(0, 0).glyph());
    feedText(terminal, "\x1b[?1047l");
}

void testCursorSaveModesAnd1049RestorePrimaryState() {
    terminal.reset();
    feedText(terminal, "\x1b[3;4H\x1b[?1048h\x1b[8;9H\x1b[?1048l");
    assertCursor(2, 3);
    feedText(terminal, "\x1b[?47h\x1b[2;3H\x1b[?1048h"
                       "\x1b[8;9H\x1b[?1048l");
    assertCursor(1, 2);
    feedText(terminal, "\x1b[?47l");

    feedText(terminal, "\x1b[31;44;1m\x1b[2;10r\x1b[4;5HP\x1b[?25l");
    const app::TerminalCursor savedPrimary = terminal.primaryScreen().cursor();
    feedText(terminal, "\x1b[?1049h");
    TEST_ASSERT_TRUE(terminal.alternateActive());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.alternateScreen().marginTop());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalRows - 1,
                            terminal.alternateScreen().marginBottom());
    TEST_ASSERT_FALSE(terminal.alternateScreen().cursor().visible);
    feedText(terminal, "X");
    TEST_ASSERT_EQUAL_UINT8(1, terminal.alternateScreen().cell(0, 0).foreground());
    TEST_ASSERT_EQUAL_UINT8(4, terminal.alternateScreen().cell(0, 0).background());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalBold,
                            terminal.alternateScreen().cell(0, 0).attributes());
    terminal.clearDirtyRows();
    const uint32_t beforeRepeated1049 = terminal.generation();
    feedText(terminal, "\x1b[?1049h");
    TEST_ASSERT_EQUAL_UINT32(beforeRepeated1049, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());
    TEST_ASSERT_EQUAL_HEX16('X', terminal.alternateScreen().cell(0, 0).glyph());

    feedText(terminal, "\x1b[32m\x1b[3;9rOLD\x1b[?1049l");
    TEST_ASSERT_FALSE(terminal.alternateActive());
    TEST_ASSERT_EQUAL_HEX16('P', terminal.primaryScreen().cell(3, 4).glyph());
    TEST_ASSERT_EQUAL_UINT8(savedPrimary.row, terminal.primaryScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(savedPrimary.column,
                            terminal.primaryScreen().cursor().column);
    TEST_ASSERT_EQUAL_UINT8(1, terminal.primaryScreen().marginTop());
    TEST_ASSERT_EQUAL_UINT8(9, terminal.primaryScreen().marginBottom());

    feedText(terminal, "\x1b[?1049h");
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.alternateScreen().cell(0, 0).glyph());
    TEST_ASSERT_EQUAL_UINT8(0, terminal.alternateScreen().marginTop());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalRows - 1,
                            terminal.alternateScreen().marginBottom());
    TEST_ASSERT_FALSE(terminal.alternateScreen().cursor().visible);
    feedText(terminal, "Z");
    TEST_ASSERT_EQUAL_UINT8(2, terminal.alternateScreen().cell(0, 0).foreground());
    feedText(terminal, "\x1b[?1049l");
    terminal.clearDirtyRows();
    const uint32_t beforeRepeated1049Leave = terminal.generation();
    feedText(terminal, "\x1b[?1049l");
    TEST_ASSERT_EQUAL_UINT32(beforeRepeated1049Leave, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());
}

void testMixedAlternateModesKeep1049CursorOwnershipBounded() {
    terminal.reset();
    feedText(terminal, "\x1b[4;5H");
    const app::TerminalCursor before47 = terminal.primaryScreen().cursor();
    feedText(terminal, "\x1b[?47h\x1b[9;10H\x1b[?1049l");
    TEST_ASSERT_FALSE(terminal.alternateActive());
    TEST_ASSERT_EQUAL_UINT8(before47.row, terminal.primaryScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(before47.column,
                            terminal.primaryScreen().cursor().column);

    terminal.reset();
    feedText(terminal, "\x1b[6;7H");
    const app::TerminalCursor before1049 = terminal.primaryScreen().cursor();
    feedText(terminal, "\x1b[?1049h\x1b[11;12H\x1b[?47l");
    TEST_ASSERT_FALSE(terminal.alternateActive());
    TEST_ASSERT_EQUAL_UINT8(before1049.row, terminal.primaryScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(before1049.column,
                            terminal.primaryScreen().cursor().column);
    feedText(terminal, "\x1b[8;9H");
    const uint32_t afterMove = terminal.generation();
    feedText(terminal, "\x1b[?1049l");
    TEST_ASSERT_EQUAL_UINT32(afterMove, terminal.generation());
    assertCursor(7, 8);

    terminal.reset();
    feedText(terminal, "\x1b[5;6H\x1b[?47hOLD");
    const uint32_t beforeUpgrade = terminal.generation();
    feedText(terminal, "\x1b[?1049h");
    TEST_ASSERT_TRUE(terminal.alternateActive());
    TEST_ASSERT_EQUAL_UINT32(beforeUpgrade + 1, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(' ', terminal.alternateScreen().cell(0, 0).glyph());
    terminal.clearDirtyRows();
    const uint32_t beforeRepeatedUpgrade = terminal.generation();
    feedText(terminal, "\x1b[?1049h");
    TEST_ASSERT_EQUAL_UINT32(beforeRepeatedUpgrade, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());
    feedText(terminal, "\x1b[?1049l");
    assertCursor(4, 5);
}

void test1049RestoresSavedCursorVisibilityAcrossAllLeaveModes() {
    terminal.reset();
    feedText(terminal, "\x1b[3;4H\x1b[?1049h\x1b[?25l");
    TEST_ASSERT_FALSE(terminal.alternateScreen().cursor().visible);
    const uint32_t beforeNormalLeave = terminal.generation();
    feedText(terminal, "\x1b[?1049l");
    TEST_ASSERT_EQUAL_UINT32(beforeNormalLeave + 2, terminal.generation());
    TEST_ASSERT_TRUE(terminal.primaryScreen().cursor().visible);
    TEST_ASSERT_TRUE(terminal.alternateScreen().cursor().visible);
    assertCursor(2, 3);

    terminal.reset();
    feedText(terminal, "\x1b[?25l\x1b[5;6H\x1b[?1049h\x1b[?25h");
    TEST_ASSERT_TRUE(terminal.alternateScreen().cursor().visible);
    const uint32_t beforeMixedLeave = terminal.generation();
    feedText(terminal, "\x1b[?47l");
    TEST_ASSERT_EQUAL_UINT32(beforeMixedLeave + 2, terminal.generation());
    TEST_ASSERT_FALSE(terminal.primaryScreen().cursor().visible);
    TEST_ASSERT_FALSE(terminal.alternateScreen().cursor().visible);
    assertCursor(4, 5);

    const uint32_t beforeRedundant1049Leave = terminal.generation();
    feedText(terminal, "\x1b[?1049l");
    TEST_ASSERT_EQUAL_UINT32(beforeRedundant1049Leave, terminal.generation());
}

void testPerScreenMarginsAndAlternateHistoryIsolation() {
    terminal.reset();
    seedRows(app::kTerminalRows);
    feedText(terminal, "\r\n");
    const size_t primaryHistorySize = terminal.history().size();
    const uint32_t primaryHistoryGeneration = terminal.history().generation();
    const uint16_t primaryTop = terminal.primaryScreen().cell(0, 0).glyph();

    feedText(terminal, "\x1b[2;10r\x1b[?47h\x1b[3;9r");
    for (size_t row = 0; row < app::kTerminalRows + 4; ++row) {
        feedText(terminal, "A\r\n");
    }
    TEST_ASSERT_EQUAL_UINT(primaryHistorySize, terminal.history().size());
    TEST_ASSERT_EQUAL_UINT32(primaryHistoryGeneration,
                             terminal.history().generation());
    TEST_ASSERT_EQUAL_UINT8(2, terminal.alternateScreen().marginTop());
    TEST_ASSERT_EQUAL_UINT8(8, terminal.alternateScreen().marginBottom());

    feedText(terminal, "\x1b[?47l");
    TEST_ASSERT_EQUAL_UINT8(1, terminal.primaryScreen().marginTop());
    TEST_ASSERT_EQUAL_UINT8(9, terminal.primaryScreen().marginBottom());
    TEST_ASSERT_EQUAL_HEX16(primaryTop, terminal.primaryScreen().cell(0, 0).glyph());
}

void testTerminalRepliesAreBoundedOrderedAndNonMutating() {
    terminal.reset();
    feedText(terminal, "\x1b[3;4H");
    terminal.clearDirtyRows();
    const uint32_t beforeQueries = terminal.generation();
    feedText(terminal, "\x1b[c\x1b[0c\x1b[5n\x1b[6n");
    TEST_ASSERT_EQUAL_UINT32(beforeQueries, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());
    assertCursor(2, 3);
    TEST_ASSERT_FALSE(terminal.replyOverflowed());

    app::TerminalReply reply;
    const uint8_t da[] = {0x1b, '[', '?', '1', ';', '2', 'c'};
    const uint8_t status[] = {0x1b, '[', '0', 'n'};
    const uint8_t cursor[] = {0x1b, '[', '3', ';', '4', 'R'};
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, da, sizeof(da));
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, da, sizeof(da));
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, status, sizeof(status));
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, cursor, sizeof(cursor));
    TEST_ASSERT_FALSE(terminal.popReply(reply));

    feedText(terminal, "\x1b[c\x1b[5n\x1b[6n\x1b[c\x1b[5n");
    TEST_ASSERT_TRUE(terminal.replyOverflowed());
    for (size_t index = 0; index < app::kTerminalReplyCapacity; ++index) {
        TEST_ASSERT_TRUE(terminal.popReply(reply));
    }
    TEST_ASSERT_FALSE(terminal.popReply(reply));
    TEST_ASSERT_TRUE(terminal.replyOverflowed());

    terminal.reset();
    TEST_ASSERT_FALSE(terminal.replyOverflowed());
    TEST_ASSERT_FALSE(terminal.popReply(reply));
}

void testReplyFifoWrapInvalidQueriesAndMaximumCursorReport() {
    terminal.reset();
    feedText(terminal, "\x1b[c\x1b[5n\x1b[6n\x1b[c");
    app::TerminalReply reply;
    const uint8_t da[] = {0x1b, '[', '?', '1', ';', '2', 'c'};
    const uint8_t status[] = {0x1b, '[', '0', 'n'};
    const uint8_t cursorHome[] = {0x1b, '[', '1', ';', '1', 'R'};
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, da, sizeof(da));
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, status, sizeof(status));

    feedText(terminal, "\x1b[5n\x1b[6n");
    TEST_ASSERT_FALSE(terminal.replyOverflowed());
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, cursorHome, sizeof(cursorHome));
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, da, sizeof(da));
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, status, sizeof(status));
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, cursorHome, sizeof(cursorHome));
    TEST_ASSERT_FALSE(terminal.popReply(reply));

    terminal.reset();
    terminal.clearDirtyRows();
    const uint32_t beforeInvalidQueries = terminal.generation();
    feedText(terminal, "\x1b[?6n\x1b[4n\x1b[1c\x1b[?c");
    TEST_ASSERT_FALSE(terminal.popReply(reply));
    TEST_ASSERT_FALSE(terminal.replyOverflowed());
    TEST_ASSERT_EQUAL_UINT32(beforeInvalidQueries, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());
    assertCursor(0, 0);

    feedText(terminal, "\x1b[13;40H");
    terminal.clearDirtyRows();
    const uint32_t beforeMaximumQuery = terminal.generation();
    feedText(terminal, "\x1b[6n");
    const uint8_t maximumCursor[] = {0x1b, '[', '1', '3', ';', '4', '0', 'R'};
    TEST_ASSERT_TRUE(terminal.popReply(reply));
    assertReply(reply, maximumCursor, sizeof(maximumCursor));
    TEST_ASSERT_FALSE(terminal.popReply(reply));
    TEST_ASSERT_EQUAL_UINT32(beforeMaximumQuery, terminal.generation());
    TEST_ASSERT_EQUAL_HEX16(0, terminal.dirtyRows());
    assertCursor(kLastRow, kLastColumn);
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testBasicTextControlsAndChunking);
    RUN_TEST(testEveryByteCanArriveInItsOwnChunk);
    RUN_TEST(testTabBackspaceAndLineControlsStayBounded);
    RUN_TEST(testTabAtLastColumnCancelsPendingWrap);
    RUN_TEST(testMalformedAndUnsupportedUtf8UseReplacement);
    RUN_TEST(testInvalidContinuationReprocessesEscapeAndControlBytes);
    RUN_TEST(testExactConfiguredColumnWrap);
    RUN_TEST(testFullScreenLineFeedEntersPrimaryHistory);
    RUN_TEST(testEscIndexNextLineReverseIndexAndCursorSaveRestore);
    RUN_TEST(testCursorRestoreCancelsPendingWrapWithoutVisibleGenerationChange);
    RUN_TEST(testOscAndDcsCancelControlsWinAfterEscapeAcrossChunks);
    RUN_TEST(testResetAndDiscardStatesNeverPrintSequencePayloads);
    RUN_TEST(testAccessorsGenerationAndDirtyRowsTrackVisibleChanges);
    RUN_TEST(testCsiCursorMovementDefaultsCoordinatesAndClamping);
    RUN_TEST(testCsiEraseLineModesUseCurrentStyle);
    RUN_TEST(testCsiEraseDisplayModesAndHistoryOnlyMode);
    RUN_TEST(testCsiInsertDeleteCharactersAndStylePreservingBlanks);
    RUN_TEST(testCsiInsertDeleteLinesRespectMarginsAndStyle);
    RUN_TEST(testCsiScrollPreservesEveryFullScreenHistoryRow);
    RUN_TEST(testCsiPartialScrollAndMarginValidation);
    RUN_TEST(testSgrAttributesAndAllColorForms);
    RUN_TEST(testSgrEveryStandardAndBrightColor);
    RUN_TEST(testMalformedExtendedSgrIsAtomicAndRecovers);
    RUN_TEST(testCompleteExtendedSgrConsumesPayloadAndAllowsLaterCodes);
    RUN_TEST(testDecSpecialGraphicsMapsEveryRequestedCharacter);
    RUN_TEST(testCsiParserBoundsDiscardMalformedSequences);
    RUN_TEST(testCsiChunkingAndC0InterruptionPreserveParserState);
    RUN_TEST(testRisResetsStyleGraphicsParserAndMargins);
    RUN_TEST(testPrivateModesApplyAllParametersAndDriveScreenBehavior);
    RUN_TEST(testScreenSelectionPreservesStateAndNoOpSwitchesStayQuiet);
    RUN_TEST(testCursorSaveModesAnd1049RestorePrimaryState);
    RUN_TEST(testMixedAlternateModesKeep1049CursorOwnershipBounded);
    RUN_TEST(test1049RestoresSavedCursorVisibilityAcrossAllLeaveModes);
    RUN_TEST(testPerScreenMarginsAndAlternateHistoryIsolation);
    RUN_TEST(testTerminalRepliesAreBoundedOrderedAndNonMutating);
    RUN_TEST(testReplyFifoWrapInvalidQueriesAndMaximumCursorReport);
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
