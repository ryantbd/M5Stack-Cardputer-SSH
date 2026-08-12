#pragma once

#include <stddef.h>
#include <stdint.h>

namespace app {

using PhysicalKeyId = uint8_t;
constexpr size_t kKeyboardRows = 4;
constexpr size_t kKeyboardColumns = 14;
constexpr size_t kPhysicalKeyCount = kKeyboardRows * kKeyboardColumns;

constexpr PhysicalKeyId keyId(uint8_t row, uint8_t column) {
    return static_cast<PhysicalKeyId>(row * kKeyboardColumns + column);
}

enum class LayerValue : uint8_t {
    None,
    Printable,
    Fn,
    Aa,
    Ctrl,
    Opt,
    Alt,
    Tab,
    Enter,
    Backspace,
    DeleteForward,
    Escape,
    Up,
    Down,
    Left,
    Right,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

struct KeyMeaning {
    char base;
    char aa;
    LayerValue fn;
    LayerValue role;
};

constexpr PhysicalKeyId kKeyFn = keyId(2, 0);
constexpr PhysicalKeyId kKeyAa = keyId(2, 1);
constexpr PhysicalKeyId kKeyCtrl = keyId(3, 0);
constexpr PhysicalKeyId kKeyOpt = keyId(3, 1);
constexpr PhysicalKeyId kKeyAlt = keyId(3, 2);
constexpr PhysicalKeyId kKeyTab = keyId(1, 0);
constexpr PhysicalKeyId kKeyEnter = keyId(2, 13);
constexpr PhysicalKeyId kKeyDelete = keyId(0, 13);
constexpr PhysicalKeyId kKeySpace = keyId(3, 13);
constexpr PhysicalKeyId kKeyBacktick = keyId(0, 0);
constexpr PhysicalKeyId kKeyUpLegend = keyId(2, 11);
constexpr PhysicalKeyId kKeyLeftLegend = keyId(3, 10);
constexpr PhysicalKeyId kKeyDownLegend = keyId(3, 11);
constexpr PhysicalKeyId kKeyRightLegend = keyId(3, 12);

const KeyMeaning& keyMeaning(PhysicalKeyId id);
bool isModifierKey(PhysicalKeyId id);

static_assert(kKeyboardRows == 4 && kKeyboardColumns == 14 && kPhysicalKeyCount == 56,
              "Cardputer ADV keyboard geometry changed");

}  // namespace app
