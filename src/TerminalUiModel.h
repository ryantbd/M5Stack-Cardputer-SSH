#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TerminalGeometry.h"

namespace app {

constexpr uint16_t kTerminalRemoteColor = 0x07e0;
constexpr uint16_t kTerminalLocalColor = 0xfd20;
constexpr uint16_t kTerminalErrorColor = 0xf800;
constexpr uint16_t kTerminalBodyX = 0;
constexpr uint16_t kTerminalRailY =
    kTerminalStatusHeight + kTerminalRows * kTerminalCellHeight;
constexpr uint8_t kTerminalScrollIndicatorWidth = 1;
constexpr uint16_t kTerminalScrollIndicatorX = 239;
constexpr uint8_t kTerminalStatusInset = 3;
constexpr uint8_t kTerminalStatusGap = 3;
constexpr uint8_t kTerminalStatusCharacterWidth = 6;

constexpr uint16_t terminalCellX(size_t column) {
    return static_cast<uint16_t>(kTerminalBodyX +
                                 column * kTerminalCellWidth);
}

constexpr uint16_t terminalCellY(size_t row) {
    return static_cast<uint16_t>(kTerminalStatusHeight +
                                 row * kTerminalCellHeight);
}

static_assert(kTerminalRailY == 132, "Terminal rail must begin at y=132");
static_assert(terminalCellX(0) == 0 && terminalCellX(39) == 234,
              "Terminal columns must occupy x=0..239");
static_assert(terminalCellY(0) == 15 && terminalCellY(12) == 123,
              "Terminal rows must occupy y=15..131");

struct TerminalModifierState {
    bool aaHeld = false;
    bool aaLocked = false;
    bool ctrlHeld = false;
    bool altHeld = false;
    bool optHeld = false;
};

const char* terminalModifierLabel(const TerminalModifierState& state);
const char* terminalModeLabel(bool localMode);
uint16_t terminalModeColor(bool localMode, bool verified);
bool terminalShowNew(bool localMode, bool alternateSnapshot,
                     bool hiddenOutput);
const char* terminalRemoteShortcutText();
const char* terminalLocalShortcutFirstText();
const char* terminalLocalShortcutSecondText();
size_t terminalRemoteShortcutRow();
size_t terminalLocalShortcutFirstRow();
size_t terminalLocalShortcutSecondRow();

struct TerminalStatusLayout {
    uint16_t modeX = kTerminalStatusInset;
    uint16_t rightX = 0;
    uint16_t hostX = 0;
    uint16_t hostPixels = 0;
};

TerminalStatusLayout terminalStatusLayout(uint16_t displayWidth,
                                          uint16_t modePixels,
                                          uint16_t rightPixels);

struct TerminalHostClip {
    uint8_t characters = 0;
    uint16_t pixelWidth = 0;
    bool clipped = false;
};

TerminalHostClip terminalHostClip(const char* host, uint16_t pixelBudget);
bool terminalHostVisible(const TerminalHostClip& clip);

bool terminalRoundedBoxMask(uint16_t glyph, uint8_t columns[5]);

class TerminalUiModel {
public:
    void showShortcutsAt(uint32_t nowMs);
    bool shortcutsVisible(uint32_t nowMs, bool optHeld) const;

private:
    uint32_t shortcutsShownAt_ = 0;
    bool shortcutsShown_ = false;
};

}  // namespace app
