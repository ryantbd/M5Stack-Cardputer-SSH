#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "TerminalEmulator.h"
#include "TerminalViewport.h"

void setUp() {}
void tearDown() {}

namespace {

app::TerminalEmulator terminal;
app::TerminalViewport viewport;
constexpr size_t kLastRow = app::kTerminalRows - 1;

static_assert(sizeof(app::TerminalReply) == app::kTerminalReplyBytes + 1,
              "Terminal replies must remain byte-packed");
static_assert(sizeof(app::TerminalEmulator) + sizeof(app::TerminalViewport) <=
                  50u * 1024u,
              "Terminal and LOCAL viewport must fit the 50 KB RAM budget");

void feedText(const char* text) {
    size_t length = 0;
    while (text[length] != '\0') ++length;
    terminal.feed(reinterpret_cast<const uint8_t*>(text), length);
}

void feedGlyph(uint8_t glyph) { terminal.feed(&glyph, 1); }

void seedVisibleRows() {
    terminal.reset();
    viewport.leaveLocal();
    for (size_t row = 0; row < app::kTerminalRows; ++row) {
        feedGlyph(static_cast<uint8_t>('A' + row));
        if (row + 1 < app::kTerminalRows) feedText("\r\n");
    }
}

void appendRow(uint8_t glyph) {
    feedText("\r\n");
    feedGlyph(glyph);
}

void fillHistory(size_t count) {
    for (size_t index = 0; index < count; ++index) {
        appendRow(static_cast<uint8_t>('P' + index % 10));
    }
}

void testPrimaryViewportCombinesHistoryAndScreen() {
    seedVisibleRows();
    appendRow('P');
    appendRow('Q');
    TEST_ASSERT_EQUAL_UINT(2, terminal.history().size());

    viewport.enterLocal(terminal);
    TEST_ASSERT_FALSE(viewport.alternateSnapshot());
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16('C', viewport.cell(terminal, 0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('Q',
                            viewport.cell(terminal, kLastRow, 0).glyph());

    viewport.scrollLines(1, terminal);
    TEST_ASSERT_EQUAL_UINT(1, viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16('B', viewport.cell(terminal, 0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('P',
                            viewport.cell(terminal, kLastRow, 0).glyph());

    appendRow('R');
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_EQUAL_UINT(2, viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16('B', viewport.cell(terminal, 0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('P',
                            viewport.cell(terminal, kLastRow, 0).glyph());
}

void testPrimaryViewportBottomPagesAndExtremeClamps() {
    seedVisibleRows();
    fillHistory(40);
    viewport.enterLocal(terminal);

    viewport.scrollPages(1, terminal);
    TEST_ASSERT_EQUAL_UINT(app::kTerminalRows, viewport.distanceFromBottom());
    viewport.scrollPages(-1, terminal);
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());

    viewport.scrollLines(INT_MAX, terminal);
    TEST_ASSERT_EQUAL_UINT(40, viewport.distanceFromBottom());
    viewport.scrollLines(INT_MIN, terminal);
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());

    viewport.scrollPages(INT_MAX, terminal);
    TEST_ASSERT_EQUAL_UINT(40, viewport.distanceFromBottom());
    viewport.scrollPages(INT_MIN, terminal);
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());

    appendRow('Z');
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16(terminal.primaryScreen().cell(0, 0).glyph(),
                            viewport.cell(terminal, 0, 0).glyph());
    viewport.scrollLines(5, terminal);
    viewport.jumpToBottom();
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());
}

void testPrimaryViewportAnchorsAcrossFullRingEviction() {
    seedVisibleRows();
    fillHistory(app::kTerminalHistoryRows);
    TEST_ASSERT_EQUAL_UINT(app::kTerminalHistoryRows, terminal.history().size());
    viewport.enterLocal(terminal);
    viewport.scrollLines(INT_MAX, terminal);
    const uint16_t evictedGlyph = viewport.cell(terminal, 0, 0).glyph();
    const uint16_t nextOldestGlyph = terminal.history().row(1)[0].glyph();

    appendRow('Z');
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_EQUAL_UINT(app::kTerminalHistoryRows,
                           viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16(nextOldestGlyph, viewport.cell(terminal, 0, 0).glyph());
    TEST_ASSERT_NOT_EQUAL(evictedGlyph, viewport.cell(terminal, 0, 0).glyph());

    const uint32_t wrappedDelta = static_cast<uint32_t>(1u - UINT32_MAX);
    TEST_ASSERT_EQUAL_UINT32(2, wrappedDelta);
}

void testPrimaryViewportClampsAfterHistoryClearAndReset() {
    seedVisibleRows();
    fillHistory(20);
    viewport.enterLocal(terminal);
    viewport.scrollLines(10, terminal);
    TEST_ASSERT_EQUAL_UINT(10, viewport.distanceFromBottom());

    feedText("\x1b[3J");
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_EQUAL_UINT(0, terminal.history().size());
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16(terminal.primaryScreen().cell(0, 0).glyph(),
                            viewport.cell(terminal, 0, 0).glyph());

    terminal.reset();
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16(' ', viewport.cell(terminal, 0, 0).glyph());

    for (size_t row = 0; row < app::kTerminalRows; ++row) {
        feedGlyph(static_cast<uint8_t>('a' + row));
        if (row + 1 < app::kTerminalRows) feedText("\r\n");
    }
    fillHistory(10);
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_EQUAL_UINT(10, terminal.history().size());
    TEST_ASSERT_TRUE(terminal.history().size() <= app::kTerminalHistoryRows);
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16(terminal.primaryScreen().cell(0, 0).glyph(),
                            viewport.cell(terminal, 0, 0).glyph());
}

void testPrimaryLocalStaysPrimaryWhenRemoteEntersAlternate() {
    seedVisibleRows();
    appendRow('P');
    appendRow('Q');
    viewport.enterLocal(terminal);
    viewport.scrollLines(1, terminal);
    const uint16_t frozenPrimaryTop = viewport.cell(terminal, 0, 0).glyph();

    feedText("\x1b[?47hLIVE");
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_TRUE(terminal.alternateActive());
    TEST_ASSERT_FALSE(viewport.alternateSnapshot());
    TEST_ASSERT_FALSE(viewport.hasNewOutput());
    TEST_ASSERT_EQUAL_UINT(1, viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16(frozenPrimaryTop,
                            viewport.cell(terminal, 0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('L', terminal.activeScreen().cell(0, 0).glyph());

    viewport.leaveLocal();
    TEST_ASSERT_FALSE(viewport.alternateSnapshot());
    TEST_ASSERT_FALSE(viewport.hasNewOutput());
    TEST_ASSERT_EQUAL_HEX16('L', viewport.cell(terminal, 0, 0).glyph());
}

void testAlternateLocalIsFrozenAndReportsNewOutput() {
    terminal.reset();
    viewport.leaveLocal();
    feedText("primary\x1b[?47hALT");
    viewport.enterLocal(terminal);
    TEST_ASSERT_TRUE(viewport.alternateSnapshot());
    TEST_ASSERT_FALSE(viewport.hasNewOutput());
    TEST_ASSERT_EQUAL_HEX16('A', viewport.cell(terminal, 0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('T', viewport.cell(terminal, 0, 2).glyph());

    feedText("\x1b[HNEW");
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_TRUE(viewport.hasNewOutput());
    TEST_ASSERT_EQUAL_HEX16('A', viewport.cell(terminal, 0, 0).glyph());
    TEST_ASSERT_EQUAL_HEX16('N', terminal.activeScreen().cell(0, 0).glyph());
    viewport.scrollLines(20, terminal);
    viewport.scrollPages(2, terminal);
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());
    TEST_ASSERT_EQUAL_HEX16('A', viewport.cell(terminal, 0, 0).glyph());

    viewport.leaveLocal();
    TEST_ASSERT_FALSE(viewport.alternateSnapshot());
    TEST_ASSERT_FALSE(viewport.hasNewOutput());
    feedText("\x1b[Hlive");
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_FALSE(viewport.hasNewOutput());

    viewport.enterLocal(terminal);
    TEST_ASSERT_TRUE(viewport.alternateSnapshot());
    TEST_ASSERT_EQUAL_HEX16('l', viewport.cell(terminal, 0, 0).glyph());
    TEST_ASSERT_FALSE(viewport.hasNewOutput());
    feedText("\x1b[?47l");
    viewport.onTerminalChanged(terminal);
    TEST_ASSERT_TRUE(viewport.hasNewOutput());
    TEST_ASSERT_EQUAL_HEX16('l', viewport.cell(terminal, 0, 0).glyph());

    viewport.leaveLocal();
    viewport.enterLocal(terminal);
    TEST_ASSERT_FALSE(viewport.alternateSnapshot());
    TEST_ASSERT_EQUAL_HEX16('p', viewport.cell(terminal, 0, 0).glyph());
}

void testViewportInvalidCellsAreBoundedBlanks() {
    seedVisibleRows();
    viewport.enterLocal(terminal);
    const app::TerminalCell& badRow = viewport.cell(terminal, app::kTerminalRows, 0);
    const app::TerminalCell& badColumn = viewport.cell(terminal, 0, app::kTerminalColumns);
    TEST_ASSERT_EQUAL_HEX16(' ', badRow.glyph());
    TEST_ASSERT_EQUAL_HEX16(' ', badColumn.glyph());
    TEST_ASSERT_EQUAL_UINT8(7, badRow.foreground());
    TEST_ASSERT_EQUAL_UINT8(0, badColumn.background());
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testPrimaryViewportCombinesHistoryAndScreen);
    RUN_TEST(testPrimaryViewportBottomPagesAndExtremeClamps);
    RUN_TEST(testPrimaryViewportAnchorsAcrossFullRingEviction);
    RUN_TEST(testPrimaryViewportClampsAfterHistoryClearAndReset);
    RUN_TEST(testPrimaryLocalStaysPrimaryWhenRemoteEntersAlternate);
    RUN_TEST(testAlternateLocalIsFrozenAndReportsNewOutput);
    RUN_TEST(testViewportInvalidCellsAreBoundedBlanks);
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
