#pragma once

#include <Arduino.h>
#include <M5Cardputer.h>
#include <M5GFX.h>

#include "TerminalEmulator.h"
#include "TerminalUiModel.h"
#include "TerminalViewport.h"

namespace app {

struct UiStatus {
    const char* host = "";
    const char* modifier = "";
    int32_t rssi = 0;
    uint8_t batteryPercent = 0;
    bool localHasNewOutput = false;
};

class UiRenderer {
public:
    UiRenderer();

    bool begin();
    void progress(const String& title,
                  const String& detail = "",
                  uint16_t current = 0,
                  uint16_t total = 0,
                  bool force = false);
    void list(const String& title,
              const String* labels,
              const String* details,
              size_t count,
              size_t selected,
              size_t first,
              const String& footer,
              const String& step = "");
    void input(const String& title,
               const String& label,
               const String& visibleValue,
               size_t cursorColumn,
               bool clippedLeft,
               const String& footer,
               const String& step = "");
    void dialog(const String& title,
                const String& line1,
                const String& line2,
                const String& footer,
                uint16_t accentColor);
    void fingerprint(const String& title, const String& value, const String& footer, uint16_t accentColor);
    // Shows a long value (such as a public key) wrapped across up to 8 lines.
    void textBlock(const String& title,
                   const String& header,
                   const String& value,
                   const String& footer,
                   uint16_t accentColor);
    void invalidateTerminal();
    void terminal(const UiStatus& status,
                  const TerminalEmulator& terminal,
                  const TerminalViewport& viewport,
                  bool localMode,
                  bool verified,
                  bool shortcutsVisible,
                  bool cursorBlinkOn);

private:
    void frame(const String& title, const String& right, uint16_t color);
    static String clipped(const String& value, size_t maximum);

    M5Canvas canvas_;
    uint32_t lastProgressDraw_ = 0;
    uint32_t lastTerminalGeneration_ = 0;
    size_t lastViewportDistance_ = 0;
    uint8_t lastCursorRow_ = 0;
    bool terminalFrameValid_ = false;
    bool lastTerminalLocal_ = false;
    bool lastShortcutVisible_ = false;
    bool lastCursorVisible_ = false;
    bool lastViewportAlternate_ = false;
};

}  // namespace app
