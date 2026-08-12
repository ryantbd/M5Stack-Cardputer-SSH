#pragma once

#include <stddef.h>
#include <stdint.h>

namespace app {

constexpr size_t kTerminalColumns = 40;
constexpr size_t kTerminalRows = 13;
constexpr size_t kTerminalHistoryRows = 120;
constexpr uint8_t kTerminalCellWidth = 6;
constexpr uint8_t kTerminalCellHeight = 9;
constexpr uint8_t kTerminalStatusHeight = 15;
constexpr uint8_t kTerminalRailHeight = 3;
constexpr uint8_t kTerminalMaxCsiParameters = 16;
constexpr uint8_t kTerminalReplyCapacity = 4;
constexpr uint8_t kTerminalReplyBytes = 16;
constexpr uint32_t kTerminalShortcutDurationMs = 4000;

static_assert(kTerminalStatusHeight + kTerminalRows * kTerminalCellHeight +
                  kTerminalRailHeight ==
              135,
              "Terminal geometry must fill the 135-pixel display");
static_assert(kTerminalColumns * kTerminalCellWidth == 240,
              "Terminal columns must fill the 240-pixel display");
static_assert(kTerminalRows < 16,
              "Terminal dirty rows must fit the 16-bit mask");

}  // namespace app
