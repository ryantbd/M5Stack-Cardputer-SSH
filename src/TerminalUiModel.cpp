#include "TerminalUiModel.h"

namespace app {

const char* terminalModifierLabel(const TerminalModifierState& state) {
    if (state.optHeld) {
        return "OPT";
    }
    if (state.ctrlHeld) {
        return "CTRL";
    }
    if (state.altHeld) {
        return "ALT";
    }
    if (state.aaLocked) {
        return "AA";
    }
    if (state.aaHeld) {
        return "Aa";
    }
    return "";
}

const char* terminalModeLabel(bool localMode) {
    return localMode ? "LOCAL" : "REMOTE";
}

uint16_t terminalModeColor(bool localMode, bool verified) {
    if (!verified) {
        return kTerminalErrorColor;
    }
    return localMode ? kTerminalLocalColor : kTerminalRemoteColor;
}

bool terminalShowNew(bool localMode, bool alternateSnapshot,
                     bool hiddenOutput) {
    return localMode && alternateSnapshot && hiddenOutput;
}

const char* terminalRemoteShortcutText() {
    return "OPT: SPACE LOCAL | DEL END | ENTER MENU";
}

const char* terminalLocalShortcutFirstText() {
    return "FN+UP/DOWN LINE | FN+LEFT/RIGHT PAGE";
}

const char* terminalLocalShortcutSecondText() {
    return "DEL BOTTOM | OPT+SPACE REMOTE";
}

size_t terminalRemoteShortcutRow() {
    return kTerminalRows - 1;
}

size_t terminalLocalShortcutFirstRow() {
    return kTerminalRows - 2;
}

size_t terminalLocalShortcutSecondRow() {
    return kTerminalRows - 1;
}

TerminalStatusLayout terminalStatusLayout(uint16_t displayWidth,
                                          uint16_t modePixels,
                                          uint16_t rightPixels) {
    TerminalStatusLayout layout;
    const uint16_t rightInset =
        displayWidth > kTerminalStatusInset
            ? static_cast<uint16_t>(displayWidth - kTerminalStatusInset)
            : 0;
    layout.rightX = rightInset > rightPixels
                        ? static_cast<uint16_t>(rightInset - rightPixels)
                        : 0;

    const uint32_t hostX = static_cast<uint32_t>(layout.modeX) + modePixels +
                           kTerminalStatusGap;
    layout.hostX = hostX > UINT16_MAX ? UINT16_MAX
                                     : static_cast<uint16_t>(hostX);
    if (layout.rightX > layout.hostX + kTerminalStatusGap) {
        layout.hostPixels = static_cast<uint16_t>(
            layout.rightX - kTerminalStatusGap - layout.hostX);
    }
    return layout;
}

TerminalHostClip terminalHostClip(const char* host, uint16_t pixelBudget) {
    TerminalHostClip result;
    if (host == nullptr) {
        return result;
    }

    size_t length = 0;
    while (host[length] != '\0' && length < UINT8_MAX) {
        ++length;
    }
    const bool boundedLength =
        length == UINT8_MAX && host[length] != '\0';
    const uint32_t fullPixels =
        static_cast<uint32_t>(length) * kTerminalStatusCharacterWidth;
    if (!boundedLength && fullPixels <= pixelBudget) {
        result.characters = static_cast<uint8_t>(length);
        result.pixelWidth = static_cast<uint16_t>(fullPixels);
        return result;
    }

    result.clipped = length != 0;
    if (!result.clipped || pixelBudget == 0) {
        return result;
    }
    if (pixelBudget < kTerminalStatusCharacterWidth) {
        result.pixelWidth = pixelBudget;
        return result;
    }

    const uint16_t availableCharacters = static_cast<uint16_t>(
        pixelBudget / kTerminalStatusCharacterWidth - 1U);
    result.characters = static_cast<uint8_t>(
        availableCharacters < length ? availableCharacters : length);
    result.pixelWidth = static_cast<uint16_t>(
        (static_cast<uint16_t>(result.characters) + 1U) *
        kTerminalStatusCharacterWidth);
    return result;
}

bool terminalHostVisible(const TerminalHostClip& clip) {
    return clip.pixelWidth != 0;
}

bool terminalRoundedBoxMask(uint16_t glyph, uint8_t columns[5]) {
    if (columns == nullptr) {
        return false;
    }
    for (size_t column = 0; column < 5; ++column) {
        columns[column] = 0;
    }

    switch (glyph) {
        case 0x256d:
            columns[2] = 0xe0;
            columns[3] = 0x10;
            columns[4] = 0x08;
            return true;
        case 0x256e:
            columns[0] = 0x08;
            columns[1] = 0x10;
            columns[2] = 0xe0;
            return true;
        case 0x256f:
            columns[0] = 0x08;
            columns[1] = 0x08;
            columns[2] = 0x07;
            return true;
        case 0x2570:
            columns[2] = 0x07;
            columns[3] = 0x08;
            columns[4] = 0x08;
            return true;
        default:
            return false;
    }
}

void TerminalUiModel::showShortcutsAt(uint32_t nowMs) {
    shortcutsShownAt_ = nowMs;
    shortcutsShown_ = true;
}

bool TerminalUiModel::shortcutsVisible(uint32_t nowMs, bool optHeld) const {
    return optHeld ||
           (shortcutsShown_ &&
            static_cast<uint32_t>(nowMs - shortcutsShownAt_) <
                kTerminalShortcutDurationMs);
}

}  // namespace app
