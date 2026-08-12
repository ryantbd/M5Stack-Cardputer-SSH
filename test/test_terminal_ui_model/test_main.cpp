#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <limits.h>
#include <string.h>

#include "TerminalGeometry.h"
#include "TerminalUiModel.h"

void setUp() {}
void tearDown() {}

namespace {

void test_shortcut_overlay_timing_and_wrap() {
    app::TerminalUiModel model;
    TEST_ASSERT_FALSE(model.shortcutsVisible(0, false));
    TEST_ASSERT_TRUE(model.shortcutsVisible(123, true));

    model.showShortcutsAt(1000);
    TEST_ASSERT_TRUE(model.shortcutsVisible(4999, false));
    TEST_ASSERT_FALSE(model.shortcutsVisible(5000, false));
    TEST_ASSERT_TRUE(model.shortcutsVisible(8000, true));

    model.showShortcutsAt(UINT32_MAX - 1000U);
    TEST_ASSERT_TRUE(model.shortcutsVisible(2998U, false));
    TEST_ASSERT_FALSE(model.shortcutsVisible(2999U, false));
}

void test_modifier_priority_and_all_combinations() {
    app::TerminalModifierState modifiers;
    TEST_ASSERT_EQUAL_STRING("", app::terminalModifierLabel(modifiers));

    modifiers.aaHeld = true;
    TEST_ASSERT_EQUAL_STRING("Aa", app::terminalModifierLabel(modifiers));
    modifiers.aaLocked = true;
    TEST_ASSERT_EQUAL_STRING("AA", app::terminalModifierLabel(modifiers));
    modifiers.altHeld = true;
    TEST_ASSERT_EQUAL_STRING("ALT", app::terminalModifierLabel(modifiers));
    modifiers.ctrlHeld = true;
    TEST_ASSERT_EQUAL_STRING("CTRL", app::terminalModifierLabel(modifiers));
    modifiers.optHeld = true;
    TEST_ASSERT_EQUAL_STRING("OPT", app::terminalModifierLabel(modifiers));

    modifiers.optHeld = false;
    TEST_ASSERT_EQUAL_STRING("CTRL", app::terminalModifierLabel(modifiers));
    modifiers.ctrlHeld = false;
    TEST_ASSERT_EQUAL_STRING("ALT", app::terminalModifierLabel(modifiers));
    modifiers.altHeld = false;
    TEST_ASSERT_EQUAL_STRING("AA", app::terminalModifierLabel(modifiers));
    modifiers.aaLocked = false;
    TEST_ASSERT_EQUAL_STRING("Aa", app::terminalModifierLabel(modifiers));

    for (uint8_t bits = 0; bits < 32; ++bits) {
        app::TerminalModifierState combination;
        combination.aaHeld = (bits & 0x01) != 0;
        combination.aaLocked = (bits & 0x02) != 0;
        combination.ctrlHeld = (bits & 0x04) != 0;
        combination.altHeld = (bits & 0x08) != 0;
        combination.optHeld = (bits & 0x10) != 0;
        const char* expected = combination.optHeld ? "OPT"
                               : combination.ctrlHeld ? "CTRL"
                               : combination.altHeld ? "ALT"
                               : combination.aaLocked ? "AA"
                               : combination.aaHeld ? "Aa"
                                                    : "";
        TEST_ASSERT_EQUAL_STRING(expected,
                                 app::terminalModifierLabel(combination));
    }
}

void test_mode_labels_colors_and_new_decision() {
    TEST_ASSERT_EQUAL_STRING("REMOTE", app::terminalModeLabel(false));
    TEST_ASSERT_EQUAL_STRING("LOCAL", app::terminalModeLabel(true));
    TEST_ASSERT_EQUAL_HEX16(app::kTerminalRemoteColor,
                            app::terminalModeColor(false, true));
    TEST_ASSERT_EQUAL_HEX16(app::kTerminalLocalColor,
                            app::terminalModeColor(true, true));
    TEST_ASSERT_EQUAL_HEX16(app::kTerminalErrorColor,
                            app::terminalModeColor(false, false));
    TEST_ASSERT_EQUAL_HEX16(app::kTerminalErrorColor,
                            app::terminalModeColor(true, false));

    TEST_ASSERT_TRUE(app::terminalShowNew(true, true, true));
    TEST_ASSERT_FALSE(app::terminalShowNew(false, true, true));
    TEST_ASSERT_FALSE(app::terminalShowNew(true, false, true));
    TEST_ASSERT_FALSE(app::terminalShowNew(true, true, false));
    for (uint8_t bits = 0; bits < 8; ++bits) {
        TEST_ASSERT_EQUAL(bits == 7,
                          app::terminalShowNew((bits & 1) != 0,
                                               (bits & 2) != 0,
                                               (bits & 4) != 0));
    }
}

void test_status_layout_reserves_mode_and_right_before_host() {
    const app::TerminalStatusLayout normal =
        app::terminalStatusLayout(240, 36, 60);
    TEST_ASSERT_EQUAL_UINT16(3, normal.modeX);
    TEST_ASSERT_EQUAL_UINT16(177, normal.rightX);
    TEST_ASSERT_EQUAL_UINT16(42, normal.hostX);
    TEST_ASSERT_EQUAL_UINT16(132, normal.hostPixels);

    const app::TerminalStatusLayout tiny =
        app::terminalStatusLayout(120, 18, 48);
    TEST_ASSERT_EQUAL_UINT16(69, tiny.rightX);
    TEST_ASSERT_EQUAL_UINT16(24, tiny.hostX);
    TEST_ASSERT_EQUAL_UINT16(42, tiny.hostPixels);

    const app::TerminalStatusLayout impossible =
        app::terminalStatusLayout(20, 36, 78);
    TEST_ASSERT_EQUAL_UINT16(0, impossible.hostPixels);

    const app::TerminalHostClip whole = app::terminalHostClip("Ubuntu001", 54);
    TEST_ASSERT_EQUAL_UINT8(9, whole.characters);
    TEST_ASSERT_FALSE(whole.clipped);
    const app::TerminalHostClip clipped = app::terminalHostClip("Ubuntu001", 18);
    TEST_ASSERT_EQUAL_UINT8(2, clipped.characters);
    TEST_ASSERT_TRUE(clipped.clipped);
    TEST_ASSERT_EQUAL_UINT16(18, clipped.pixelWidth);
    const app::TerminalHostClip indicatorOnly =
        app::terminalHostClip("Ubuntu001", 5);
    TEST_ASSERT_EQUAL_UINT8(0, indicatorOnly.characters);
    TEST_ASSERT_TRUE(indicatorOnly.clipped);
    TEST_ASSERT_EQUAL_UINT16(5, indicatorOnly.pixelWidth);
    TEST_ASSERT_TRUE(app::terminalHostVisible(indicatorOnly));

    const app::TerminalHostClip zeroBudget =
        app::terminalHostClip("Ubuntu001", 0);
    TEST_ASSERT_EQUAL_UINT8(0, zeroBudget.characters);
    TEST_ASSERT_TRUE(zeroBudget.clipped);
    TEST_ASSERT_EQUAL_UINT16(0, zeroBudget.pixelWidth);
    TEST_ASSERT_FALSE(app::terminalHostVisible(zeroBudget));
}

void test_rounded_box_masks_connect_to_neighbors() {
    uint8_t columns[5] = {};

    TEST_ASSERT_TRUE(app::terminalRoundedBoxMask(0x256d, columns));
    const uint8_t topLeft[] = {0x00, 0x00, 0xe0, 0x10, 0x08};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(topLeft, columns, 5);
    TEST_ASSERT_BITS_HIGH(1U << 3, columns[4]);
    TEST_ASSERT_BITS_HIGH(1U << 7, columns[2]);

    TEST_ASSERT_TRUE(app::terminalRoundedBoxMask(0x256e, columns));
    const uint8_t topRight[] = {0x08, 0x10, 0xe0, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(topRight, columns, 5);
    TEST_ASSERT_BITS_HIGH(1U << 3, columns[0]);
    TEST_ASSERT_BITS_HIGH(1U << 7, columns[2]);

    TEST_ASSERT_TRUE(app::terminalRoundedBoxMask(0x256f, columns));
    const uint8_t bottomRight[] = {0x08, 0x08, 0x07, 0x00, 0x00};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bottomRight, columns, 5);
    TEST_ASSERT_BITS_HIGH(1U << 3, columns[0]);
    TEST_ASSERT_BITS_HIGH(1U << 0, columns[2]);

    TEST_ASSERT_TRUE(app::terminalRoundedBoxMask(0x2570, columns));
    const uint8_t bottomLeft[] = {0x00, 0x00, 0x07, 0x08, 0x08};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bottomLeft, columns, 5);
    TEST_ASSERT_BITS_HIGH(1U << 3, columns[4]);
    TEST_ASSERT_BITS_HIGH(1U << 0, columns[2]);

    TEST_ASSERT_FALSE(app::terminalRoundedBoxMask(0x2500, columns));
}

void test_exact_terminal_geometry() {
    TEST_ASSERT_EQUAL_UINT16(240,
                             app::kTerminalColumns * app::kTerminalCellWidth);
    TEST_ASSERT_EQUAL_UINT16(135, app::kTerminalStatusHeight +
                                      app::kTerminalRows * app::kTerminalCellHeight +
                                      app::kTerminalRailHeight);
    TEST_ASSERT_EQUAL_UINT16(117,
                             app::kTerminalRows * app::kTerminalCellHeight);
    TEST_ASSERT_EQUAL_UINT16(0, app::terminalCellX(0));
    TEST_ASSERT_EQUAL_UINT16(234, app::terminalCellX(39));
    TEST_ASSERT_EQUAL_UINT16(15, app::terminalCellY(0));
    TEST_ASSERT_EQUAL_UINT16(123, app::terminalCellY(12));
    TEST_ASSERT_EQUAL_UINT16(132, app::kTerminalRailY);
}

void test_shortcuts_fit_large_terminal_rows() {
    TEST_ASSERT_EQUAL_UINT(39, strlen(app::terminalRemoteShortcutText()));
    TEST_ASSERT_EQUAL_UINT(36,
                           strlen(app::terminalLocalShortcutFirstText()));
    TEST_ASSERT_EQUAL_UINT(29,
                           strlen(app::terminalLocalShortcutSecondText()));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(
        app::kTerminalColumns, strlen(app::terminalRemoteShortcutText()));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(
        app::kTerminalColumns, strlen(app::terminalLocalShortcutFirstText()));
    TEST_ASSERT_LESS_OR_EQUAL_UINT(
        app::kTerminalColumns, strlen(app::terminalLocalShortcutSecondText()));
    TEST_ASSERT_EQUAL_UINT(app::kTerminalRows - 1,
                           app::terminalRemoteShortcutRow());
    TEST_ASSERT_EQUAL_UINT(app::kTerminalRows - 2,
                           app::terminalLocalShortcutFirstRow());
    TEST_ASSERT_EQUAL_UINT(app::kTerminalRows - 1,
                           app::terminalLocalShortcutSecondRow());
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_shortcut_overlay_timing_and_wrap);
    RUN_TEST(test_modifier_priority_and_all_combinations);
    RUN_TEST(test_mode_labels_colors_and_new_decision);
    RUN_TEST(test_status_layout_reserves_mode_and_right_before_host);
    RUN_TEST(test_rounded_box_masks_connect_to_neighbors);
    RUN_TEST(test_exact_terminal_geometry);
    RUN_TEST(test_shortcuts_fit_large_terminal_rows);
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
