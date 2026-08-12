#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <stddef.h>
#include <stdint.h>

#include "TerminalEmulator.h"
#include "TerminalGlyphs.h"
#include "TerminalKeyEncoder.h"
#include "TerminalFixtures.h"

void setUp() {}
void tearDown() {}

namespace {

app::TerminalEmulator terminal;

struct GoldenRun {
    uint8_t row;
    uint8_t column;
    const uint16_t* glyphs;
    uint8_t length;
    uint8_t foreground;
    uint8_t background;
    uint8_t attributes;
};

template <size_t Size>
constexpr uint8_t glyphCount(const uint16_t (&)[Size]) {
    return static_cast<uint8_t>(Size);
}

template <size_t Size>
void feedFixture(const char (&fixture)[Size]) {
    terminal.feed(reinterpret_cast<const uint8_t*>(fixture), Size - 1);
}

void feedText(const char* text) {
    size_t length = 0;
    while (text[length] != '\0') ++length;
    terminal.feed(reinterpret_cast<const uint8_t*>(text), length);
}

void assertGoldenScreen(const app::TerminalScreen& screen,
                        const GoldenRun* runs, size_t runCount) {
    for (size_t row = 0; row < app::kTerminalRows; ++row) {
        for (size_t column = 0; column < app::kTerminalColumns; ++column) {
            uint16_t glyph = ' ';
            uint8_t foreground = 7;
            uint8_t background = 0;
            uint8_t attributes = 0;
            bool matched = false;
            for (size_t index = 0; index < runCount; ++index) {
                const GoldenRun& run = runs[index];
                if (run.row != row || column < run.column ||
                    column >= static_cast<size_t>(run.column) + run.length) {
                    continue;
                }
                TEST_ASSERT_FALSE_MESSAGE(matched, "Golden runs overlap");
                glyph = run.glyphs[column - run.column];
                foreground = run.foreground;
                background = run.background;
                attributes = run.attributes;
                matched = true;
            }

            const app::TerminalCell& cell = screen.cell(row, column);
            TEST_ASSERT_EQUAL_HEX16(glyph, cell.glyph());
            TEST_ASSERT_EQUAL_UINT8(foreground, cell.foreground());
            TEST_ASSERT_EQUAL_UINT8(background, cell.background());
            TEST_ASSERT_EQUAL_UINT8(attributes, cell.attributes());
        }
    }
}

constexpr uint16_t kVimTitle[] = {
    'f', 'i', 'l', 'e', '.', 't', 'x', 't',
};
constexpr uint16_t kHello[] = {'h', 'e', 'l', 'l', 'o'};
constexpr uint16_t kInsert[] = {
    '-', '-', ' ', 'I', 'N', 'S', 'E', 'R', 'T', ' ', '-', '-',
};
constexpr uint16_t kShellPrompt[] = {
    's', 'h', 'e', 'l', 'l', '$', ' ', 'v', 'i', 'm',
    ' ', 'f', 'i', 'l', 'e', '.', 't', 'x', 't',
};
constexpr uint16_t kTopTitle[] = {
    't', 'o', 'p', ' ', '-', ' ', '0', '2', ':', '4', '1',
};
constexpr uint16_t kTasks[] = {
    'T', 'a', 's', 'k', 's', ':', ' ', '1',
    '2', '4', ' ', 't', 'o', 't', 'a', 'l',
};
constexpr uint16_t kTmuxTop[] = {
    0x250c, 0x2500, 0x2500, 0x2500, 0x2500, 0x2500,
    0x2500, 0x2500, 0x2500, 0x2500, 0x2500, 0x2510,
};
constexpr uint16_t kTmuxMiddle[] = {
    0x2502, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 0x2502,
};
constexpr uint16_t kTmuxBottom[] = {
    0x2514, 0x2500, 0x2500, 0x2500, 0x2500, 0x2500,
    0x2500, 0x2500, 0x2500, 0x2500, 0x2500, 0x2518,
};
constexpr uint16_t kNanoTitle[] = {
    ' ', 'G', 'N', 'U', ' ', 'n', 'a',
    'n', 'o', ' ', '7', '.', '2', ' ',
};
constexpr uint16_t kNanoBody[] = {
    'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd',
};
constexpr uint16_t kCpu[] = {'C', 'P', 'U'};
constexpr uint16_t kBlocks[] = {0x2588, 0x2588};
constexpr uint16_t kArrow[] = {0x2192};
constexpr uint16_t kBraille[] = {0x2801};
constexpr uint16_t kLatin1[] = {0x00e9};
constexpr uint16_t kReplacement[] = {app::kTerminalReplacementGlyph};

void test_vim_golden_primary_restore_and_application_cursor() {
    terminal.reset();
    feedFixture(app::terminal_fixtures::kVimFrame);
    TEST_ASSERT_TRUE(terminal.alternateActive());
    TEST_ASSERT_TRUE(terminal.inputModes().applicationCursor);

    const GoldenRun runs[] = {
        {0, 0, kVimTitle, glyphCount(kVimTitle), 7, 0, 0},
        {1, 0, kHello, glyphCount(kHello), 196, 17, 0},
        {app::kTerminalRows - 1, 0, kInsert, glyphCount(kInsert), 7, 0, 0},
    };
    assertGoldenScreen(terminal.activeScreen(), runs,
                       sizeof(runs) / sizeof(runs[0]));
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalRows - 1,
                            terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(glyphCount(kInsert),
                            terminal.activeScreen().cursor().column);
    TEST_ASSERT_TRUE(terminal.activeScreen().cursor().visible);

    app::KeyEvent up;
    up.type = app::KeyEventType::MoveUp;
    const app::EncodedKey encoded =
        app::TerminalKeyEncoder::encode(up, terminal.inputModes());
    const uint8_t expectedUp[] = {0x1b, 'O', 'A'};
    TEST_ASSERT_EQUAL_UINT8(sizeof(expectedUp), encoded.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedUp, encoded.bytes,
                                  sizeof(expectedUp));

    feedText("\x1b[?1l\x1b[?1049l");
    TEST_ASSERT_FALSE(terminal.alternateActive());
    TEST_ASSERT_FALSE(terminal.inputModes().applicationCursor);
    const GoldenRun primaryRuns[] = {
        {0, 0, kShellPrompt, glyphCount(kShellPrompt), 7, 0, 0},
    };
    assertGoldenScreen(terminal.primaryScreen(), primaryRuns,
                       sizeof(primaryRuns) / sizeof(primaryRuns[0]));
    TEST_ASSERT_EQUAL_UINT8(1, terminal.primaryScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(0, terminal.primaryScreen().cursor().column);
}

void test_top_dynamic_refresh_has_exact_final_frame() {
    terminal.reset();
    feedFixture(app::terminal_fixtures::kTopFrame);
    TEST_ASSERT_TRUE(terminal.alternateActive());
    const GoldenRun runs[] = {
        {0, 0, kTopTitle, glyphCount(kTopTitle), 7, 0, 0},
        {1, 0, kTasks, glyphCount(kTasks), 7, 0, 0},
    };
    assertGoldenScreen(terminal.activeScreen(), runs,
                       sizeof(runs) / sizeof(runs[0]));
}

void test_tmux_dec_graphics_has_connected_box_glyphs() {
    terminal.reset();
    feedFixture(app::terminal_fixtures::kTmuxFrame);
    TEST_ASSERT_TRUE(terminal.alternateActive());
    const GoldenRun runs[] = {
        {0, 0, kTmuxTop, glyphCount(kTmuxTop), 7, 0, 0},
        {1, 0, kTmuxMiddle, glyphCount(kTmuxMiddle), 7, 0, 0},
        {2, 0, kTmuxBottom, glyphCount(kTmuxBottom), 7, 0, 0},
    };
    assertGoldenScreen(terminal.activeScreen(), runs,
                       sizeof(runs) / sizeof(runs[0]));
}

void test_nano_cursor_inverse_title_and_body_are_exact() {
    terminal.reset();
    feedFixture(app::terminal_fixtures::kNanoFrame);
    TEST_ASSERT_TRUE(terminal.alternateActive());
    const GoldenRun runs[] = {
        {0, 0, kNanoTitle, glyphCount(kNanoTitle), 7, 0,
         app::kTerminalInverse},
        {2, 0, kNanoBody, glyphCount(kNanoBody), 7, 0, 0},
    };
    assertGoldenScreen(terminal.activeScreen(), runs,
                       sizeof(runs) / sizeof(runs[0]));
    TEST_ASSERT_EQUAL_UINT8(2, terminal.activeScreen().cursor().row);
    TEST_ASSERT_EQUAL_UINT8(2, terminal.activeScreen().cursor().column);
    TEST_ASSERT_TRUE(terminal.activeScreen().cursor().visible);
}

void test_htop_256_color_and_supported_unicode_are_exact() {
    terminal.reset();
    feedFixture(app::terminal_fixtures::kHtopFrame);
    TEST_ASSERT_TRUE(terminal.alternateActive());
    const GoldenRun runs[] = {
        {0, 0, kCpu, glyphCount(kCpu), 46, 0, 0},
        {0, 4, kBlocks, glyphCount(kBlocks), 196, 0, 0},
        {1, 0, kArrow, glyphCount(kArrow), 7, 0, 0},
        {1, 2, kBraille, glyphCount(kBraille), 7, 0, 0},
        {1, 4, kLatin1, glyphCount(kLatin1), 7, 0, 0},
        {1, 6, kReplacement, glyphCount(kReplacement), 7, 0, 0},
    };
    assertGoldenScreen(terminal.activeScreen(), runs,
                       sizeof(runs) / sizeof(runs[0]));
}

uint8_t nextFuzzByte(uint32_t& state) {
    state = state * 1664525U + 1013904223U;
    return static_cast<uint8_t>(state >> 24);
}

void assertScreenBounds(const app::TerminalScreen& screen) {
    TEST_ASSERT_LESS_THAN_UINT8(app::kTerminalRows, screen.cursor().row);
    TEST_ASSERT_LESS_THAN_UINT8(app::kTerminalColumns,
                                screen.cursor().column);
    TEST_ASSERT_LESS_THAN_UINT8(app::kTerminalRows, screen.marginTop());
    TEST_ASSERT_LESS_THAN_UINT8(app::kTerminalRows, screen.marginBottom());
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(screen.marginBottom(),
                                    screen.marginTop());
    (void)screen.cell(0, 0);
    (void)screen.cell(app::kTerminalRows - 1, app::kTerminalColumns - 1);
    TEST_ASSERT_EQUAL_HEX16(' ',
                            screen.cell(app::kTerminalRows,
                                        app::kTerminalColumns).glyph());
}

void assertTerminalBounds() {
    assertScreenBounds(terminal.primaryScreen());
    assertScreenBounds(terminal.alternateScreen());
    assertScreenBounds(terminal.activeScreen());
    TEST_ASSERT_LESS_OR_EQUAL_UINT(app::kTerminalHistoryRows,
                                   terminal.history().size());
    const uint16_t visibleMask = static_cast<uint16_t>(
        (1U << app::kTerminalRows) - 1U);
    TEST_ASSERT_EQUAL_HEX16(
        0, static_cast<uint16_t>(terminal.dirtyRows() & ~visibleMask));
    TEST_ASSERT_EQUAL_HEX16(
        ' ', terminal.history().row(terminal.history().size())[0].glyph());

    app::TerminalReply reply;
    size_t replyCount = 0;
    while (terminal.popReply(reply)) {
        ++replyCount;
        TEST_ASSERT_LESS_OR_EQUAL_UINT(app::kTerminalReplyCapacity,
                                       replyCount);
        TEST_ASSERT_GREATER_THAN_UINT8(0, reply.length);
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(app::kTerminalReplyBytes,
                                        reply.length);
    }
}

void test_deterministic_malformed_streams_stay_bounded() {
    constexpr size_t kFuzzBytes = 50000;
    constexpr uint32_t kSeed = 0x5a17c0deU;

    terminal.reset();
    uint32_t state = kSeed;
    size_t generated = 0;
    size_t chunkIndex = 0;
    uint8_t chunk[11]{};
    while (generated < kFuzzBytes) {
        size_t length = chunkIndex % sizeof(chunk) + 1;
        if (length > kFuzzBytes - generated) {
            length = kFuzzBytes - generated;
        }
        for (size_t index = 0; index < length; ++index) {
            chunk[index] = nextFuzzByte(state);
        }
        terminal.feed(chunk, length);
        generated += length;
        ++chunkIndex;
        assertTerminalBounds();
    }

    terminal.reset();
    state = kSeed;
    for (size_t index = 0; index < kFuzzBytes; ++index) {
        const uint8_t byte = nextFuzzByte(state);
        terminal.feed(&byte, 1);
        assertTerminalBounds();
    }
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_vim_golden_primary_restore_and_application_cursor);
    RUN_TEST(test_top_dynamic_refresh_has_exact_final_frame);
    RUN_TEST(test_tmux_dec_graphics_has_connected_box_glyphs);
    RUN_TEST(test_nano_cursor_inverse_title_and_body_are_exact);
    RUN_TEST(test_htop_256_color_and_supported_unicode_are_exact);
    RUN_TEST(test_deterministic_malformed_streams_stay_bounded);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runTests();
}
void loop() {}
#else
int main(int, char**) { return runTests(); }
#endif
