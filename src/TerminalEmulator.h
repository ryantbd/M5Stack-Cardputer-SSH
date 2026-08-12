#pragma once

#include <stddef.h>
#include <stdint.h>

#include "TerminalGlyphs.h"
#include "TerminalHistory.h"
#include "TerminalScreen.h"

namespace app {

struct TerminalReply {
    uint8_t bytes[kTerminalReplyBytes]{};
    uint8_t length = 0;
};

class TerminalEmulator {
public:
    TerminalEmulator() = default;

    void reset();
    void feed(const uint8_t* bytes, size_t length);
    const TerminalScreen& activeScreen() const;
    const TerminalScreen& primaryScreen() const;
    const TerminalScreen& alternateScreen() const;
    const TerminalHistory& history() const;
    bool alternateActive() const;
    TerminalInputModes inputModes() const;
    uint32_t generation() const;
    uint16_t dirtyRows() const;
    void invalidateAll();
    void clearDirtyRows();
    bool popReply(TerminalReply& reply);
    bool replyOverflowed() const;

private:
    enum class ParserState : uint8_t {
        Ground,
        Escape,
        CsiEntry,
        CsiParameter,
        Osc,
        OscEscape,
        Dcs,
        DcsEscape,
    };

    TerminalScreen& activeScreenMutable();
    void processByte(uint8_t byte);
    void processGroundByte(uint8_t byte);
    void processEscapeByte(uint8_t byte);
    void processCsiByte(uint8_t byte);
    void processOscByte(uint8_t byte);
    void processDcsByte(uint8_t byte);
    bool executeControl(uint8_t byte);
    void putCodePoint(uint32_t codePoint);
    void beginCsi();
    void resetCsi();
    void appendCsiDigit(uint8_t digit);
    void appendCsiSeparator();
    void dispatchCsi(uint8_t finalByte);
    void dispatchSgr();
    void dispatchModes(bool privateModes, bool set);
    void setAlternateActive(bool active);
    void enterAlternate1049();
    void leaveAlternate1049();
    void restorePrimary1049Cursor();
    void setCursorVisible(bool visible);
    void enqueuePrimaryDeviceAttributes();
    void enqueueStatusReport();
    void enqueueCursorReport();
    bool enqueueReply(const TerminalReply& reply);
    uint16_t csiParameter(size_t index, uint16_t defaultValue) const;
    uint8_t csiCount(size_t index, uint8_t maximum) const;
    uint8_t csiCoordinate(size_t index, uint8_t maximum,
                          uint16_t defaultValue = 1) const;
    void setCursor(int row, int column);
    void setStyle(uint8_t foreground, uint8_t background, uint8_t attributes);
    void scrollUp(uint8_t count);
    void lineFeed();
    void nextLine();
    void reverseIndex();
    void saveCursor();
    void restoreCursor();
    void resetParser();
    void resetUtf8();

    TerminalScreen primary_{};
    TerminalScreen alternate_{};
    TerminalHistory history_{};
    TerminalCursor savedPrimaryCursor_{};
    TerminalCursor savedAlternateCursor_{};
    TerminalCursor savedPrimary1049Cursor_{};
    TerminalInputModes inputModes_{};
    ParserState parserState_ = ParserState::Ground;
    uint32_t utf8CodePoint_ = 0;
    uint32_t utf8Minimum_ = 0;
    uint8_t utf8Remaining_ = 0;
    bool utf8Error_ = false;
    uint8_t escapeIntermediate_ = 0;
    uint16_t csiParameters_[kTerminalMaxCsiParameters]{};
    TerminalReply replies_[kTerminalReplyCapacity]{};
    uint8_t csiParameterCount_ = 0;
    uint8_t csiDigitCount_ = 0;
    uint8_t csiPrefix_ = 0;
    uint8_t csiIntermediate_ = 0;
    bool csiParameterActive_ = false;
    bool csiMalformed_ = false;
    uint8_t foreground_ = 7;
    uint8_t background_ = 0;
    uint8_t attributes_ = 0;
    uint32_t generation_ = 0;
    uint16_t invalidatedRows_ = 0;
    uint8_t replyFirst_ = 0;
    uint8_t replyCount_ = 0;
    bool alternateActive_ = false;
    bool alternate1049Active_ = false;
    bool autowrap_ = true;
    bool insertMode_ = false;
    bool cursorVisible_ = true;
    bool decGraphics_ = false;
    bool replyOverflowed_ = false;
};

}  // namespace app
