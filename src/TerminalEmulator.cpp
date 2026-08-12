#include "TerminalEmulator.h"

#include "TerminalPalette.h"

namespace app {
namespace {

constexpr uint16_t allRowsDirtyMask() {
    return static_cast<uint16_t>((static_cast<uint32_t>(1) << kTerminalRows) - 1);
}

bool isC0Control(uint8_t byte) { return byte < 0x20 || byte == 0x7f; }

bool isCsiParameterByte(uint8_t byte) { return byte >= 0x30 && byte <= 0x3f; }

bool isCsiIntermediateByte(uint8_t byte) { return byte >= 0x20 && byte <= 0x2f; }

bool isCsiFinalByte(uint8_t byte) { return byte >= 0x40 && byte <= 0x7e; }

constexpr uint8_t kMaximumCsiDigits = 5;

uint8_t clippedByte(uint16_t value) {
    return value > 255 ? 255 : static_cast<uint8_t>(value);
}

bool appendReplyByte(TerminalReply& reply, uint8_t byte) {
    if (reply.length >= kTerminalReplyBytes) return false;
    reply.bytes[reply.length++] = byte;
    return true;
}

bool appendReplyDecimal(TerminalReply& reply, uint8_t value) {
    if (value >= 100) {
        return appendReplyByte(reply, static_cast<uint8_t>('0' + value / 100)) &&
               appendReplyByte(reply,
                               static_cast<uint8_t>('0' + (value / 10) % 10)) &&
               appendReplyByte(reply, static_cast<uint8_t>('0' + value % 10));
    }
    if (value >= 10) {
        return appendReplyByte(reply, static_cast<uint8_t>('0' + value / 10)) &&
               appendReplyByte(reply, static_cast<uint8_t>('0' + value % 10));
    }
    return appendReplyByte(reply, static_cast<uint8_t>('0' + value));
}

uint16_t decSpecialGraphicsGlyph(uint32_t codePoint) {
    switch (codePoint) {
        case 'j': return 0x2518;
        case 'k': return 0x2510;
        case 'l': return 0x250c;
        case 'm': return 0x2514;
        case 'n': return 0x253c;
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's': return 0x2500;
        case 't': return 0x251c;
        case 'u': return 0x2524;
        case 'v': return 0x2534;
        case 'w': return 0x252c;
        case 'x': return 0x2502;
        default: return normalizeTerminalCodePoint(codePoint);
    }
}

}  // namespace

void TerminalEmulator::reset() {
    primary_.reset();
    alternate_.reset();
    history_.clear();
    savedPrimaryCursor_ = {};
    savedAlternateCursor_ = {};
    savedPrimary1049Cursor_ = {};
    inputModes_ = {};
    alternateActive_ = false;
    alternate1049Active_ = false;
    autowrap_ = true;
    insertMode_ = false;
    cursorVisible_ = true;
    foreground_ = 7;
    background_ = 0;
    attributes_ = 0;
    decGraphics_ = false;
    invalidatedRows_ = 0;
    replyFirst_ = 0;
    replyCount_ = 0;
    replyOverflowed_ = false;
    for (TerminalReply& reply : replies_) reply = {};
    resetParser();
    ++generation_;
}

void TerminalEmulator::feed(const uint8_t* bytes, size_t length) {
    if (bytes == nullptr) return;
    for (size_t index = 0; index < length; ++index) processByte(bytes[index]);
}

const TerminalScreen& TerminalEmulator::activeScreen() const {
    return alternateActive_ ? alternate_ : primary_;
}

const TerminalScreen& TerminalEmulator::primaryScreen() const { return primary_; }

const TerminalScreen& TerminalEmulator::alternateScreen() const { return alternate_; }

const TerminalHistory& TerminalEmulator::history() const { return history_; }

bool TerminalEmulator::alternateActive() const { return alternateActive_; }

TerminalInputModes TerminalEmulator::inputModes() const { return inputModes_; }

uint32_t TerminalEmulator::generation() const { return generation_; }

uint16_t TerminalEmulator::dirtyRows() const {
    return static_cast<uint16_t>(activeScreen().dirtyRows() | invalidatedRows_);
}

void TerminalEmulator::invalidateAll() { invalidatedRows_ = allRowsDirtyMask(); }

void TerminalEmulator::clearDirtyRows() {
    activeScreenMutable().clearDirtyRows();
    invalidatedRows_ = 0;
}

bool TerminalEmulator::popReply(TerminalReply& reply) {
    reply = {};
    if (replyCount_ == 0) return false;
    reply = replies_[replyFirst_];
    replies_[replyFirst_] = {};
    replyFirst_ = static_cast<uint8_t>((replyFirst_ + 1) % kTerminalReplyCapacity);
    --replyCount_;
    return true;
}

bool TerminalEmulator::replyOverflowed() const { return replyOverflowed_; }

TerminalScreen& TerminalEmulator::activeScreenMutable() {
    return alternateActive_ ? alternate_ : primary_;
}

void TerminalEmulator::processByte(uint8_t byte) {
    switch (parserState_) {
        case ParserState::Ground:
            processGroundByte(byte);
            break;
        case ParserState::Escape:
            processEscapeByte(byte);
            break;
        case ParserState::CsiEntry:
        case ParserState::CsiParameter:
            processCsiByte(byte);
            break;
        case ParserState::Osc:
        case ParserState::OscEscape:
            processOscByte(byte);
            break;
        case ParserState::Dcs:
        case ParserState::DcsEscape:
            processDcsByte(byte);
            break;
    }
}

void TerminalEmulator::processGroundByte(uint8_t byte) {
    if (utf8Remaining_ != 0) {
        if (byte >= 0x80 && byte <= 0xbf) {
            utf8CodePoint_ = (utf8CodePoint_ << 6) | (byte & 0x3f);
            --utf8Remaining_;
            if (utf8Remaining_ == 0) {
                const uint32_t codePoint = utf8CodePoint_;
                utf8Error_ = codePoint < utf8Minimum_ || codePoint > 0x10ffff ||
                             (codePoint >= 0xd800 && codePoint <= 0xdfff);
                const bool invalid = utf8Error_;
                resetUtf8();
                putCodePoint(invalid ? kTerminalReplacementGlyph : codePoint);
            }
            return;
        }

        putCodePoint(kTerminalReplacementGlyph);
        resetUtf8();
        processByte(byte);
        return;
    }

    if (byte < 0x80) {
        if (byte == 0x1b) {
            escapeIntermediate_ = 0;
            parserState_ = ParserState::Escape;
        } else if (isC0Control(byte)) {
            executeControl(byte);
        } else {
            putCodePoint(byte);
        }
        return;
    }

    if (byte >= 0xc2 && byte <= 0xdf) {
        utf8CodePoint_ = byte & 0x1f;
        utf8Minimum_ = 0x80;
        utf8Remaining_ = 1;
        utf8Error_ = false;
    } else if (byte >= 0xe0 && byte <= 0xef) {
        utf8CodePoint_ = byte & 0x0f;
        utf8Minimum_ = 0x800;
        utf8Remaining_ = 2;
        utf8Error_ = false;
    } else if (byte >= 0xf0 && byte <= 0xf4) {
        utf8CodePoint_ = byte & 0x07;
        utf8Minimum_ = 0x10000;
        utf8Remaining_ = 3;
        utf8Error_ = false;
    } else {
        putCodePoint(kTerminalReplacementGlyph);
    }
}

void TerminalEmulator::processEscapeByte(uint8_t byte) {
    if (byte == 0x1b) {
        escapeIntermediate_ = 0;
        resetCsi();
        return;
    }
    if (byte == 0x18 || byte == 0x1a) {
        escapeIntermediate_ = 0;
        resetCsi();
        parserState_ = ParserState::Ground;
        return;
    }
    if (isC0Control(byte)) {
        executeControl(byte);
        return;
    }

    if (escapeIntermediate_ != 0) {
        if (isCsiIntermediateByte(byte)) {
            escapeIntermediate_ = 0xff;
            return;
        }
        const uint8_t intermediate = escapeIntermediate_;
        escapeIntermediate_ = 0;
        parserState_ = ParserState::Ground;
        if (intermediate == '(') {
            if (byte == '0') {
                decGraphics_ = true;
            } else if (byte == 'B') {
                decGraphics_ = false;
            }
        }
        return;
    }
    if (isCsiIntermediateByte(byte)) {
        escapeIntermediate_ = byte;
        return;
    }

    parserState_ = ParserState::Ground;
    switch (byte) {
        case '[':
            beginCsi();
            parserState_ = ParserState::CsiEntry;
            break;
        case ']':
            parserState_ = ParserState::Osc;
            break;
        case 'P':
            parserState_ = ParserState::Dcs;
            break;
        case 'D':
            lineFeed();
            break;
        case 'E':
            nextLine();
            break;
        case 'M':
            reverseIndex();
            break;
        case '7':
            saveCursor();
            break;
        case '8':
            restoreCursor();
            break;
        case 'c':
            reset();
            break;
        default:
            break;
    }
}

void TerminalEmulator::processCsiByte(uint8_t byte) {
    if (byte == 0x1b) {
        resetCsi();
        escapeIntermediate_ = 0;
        parserState_ = ParserState::Escape;
        return;
    }
    if (byte == 0x18 || byte == 0x1a) {
        resetCsi();
        parserState_ = ParserState::Ground;
        return;
    }
    if (isC0Control(byte)) {
        executeControl(byte);
        return;
    }
    if (isCsiParameterByte(byte)) {
        if (parserState_ == ParserState::CsiEntry) {
            parserState_ = ParserState::CsiParameter;
        }
        if (csiMalformed_) return;
        if (csiIntermediate_ != 0) {
            csiMalformed_ = true;
            return;
        }
        if (byte >= '0' && byte <= '9') {
            appendCsiDigit(static_cast<uint8_t>(byte - '0'));
        } else if (byte == ';') {
            appendCsiSeparator();
        } else if (byte >= '<' && byte <= '?') {
            if (csiPrefix_ != 0 || csiParameterCount_ != 0 ||
                csiParameterActive_) {
                csiMalformed_ = true;
            } else {
                csiPrefix_ = byte;
            }
        } else {
            csiMalformed_ = true;
        }
        return;
    }
    if (isCsiIntermediateByte(byte)) {
        if (!csiMalformed_) {
            if (csiIntermediate_ != 0) {
                csiMalformed_ = true;
            } else {
                csiIntermediate_ = byte;
            }
        }
        return;
    }
    if (isCsiFinalByte(byte)) {
        if (!csiMalformed_ && csiParameterCount_ != 0 && !csiParameterActive_) {
            if (csiParameterCount_ >= kTerminalMaxCsiParameters) {
                csiMalformed_ = true;
            } else {
                csiParameters_[csiParameterCount_++] = 0;
            }
        }
        if (!csiMalformed_) dispatchCsi(byte);
        resetCsi();
        parserState_ = ParserState::Ground;
        return;
    }
    csiMalformed_ = true;
}

void TerminalEmulator::processOscByte(uint8_t byte) {
    if (byte == 0x07 || byte == 0x18 || byte == 0x1a) {
        parserState_ = ParserState::Ground;
        return;
    }

    if (parserState_ == ParserState::OscEscape) {
        if (byte == '\\') {
            parserState_ = ParserState::Ground;
        } else if (byte != 0x1b) {
            parserState_ = ParserState::Osc;
        }
        return;
    }

    if (byte == 0x1b) {
        parserState_ = ParserState::OscEscape;
    }
}

void TerminalEmulator::processDcsByte(uint8_t byte) {
    if (byte == 0x18 || byte == 0x1a) {
        parserState_ = ParserState::Ground;
        return;
    }

    if (parserState_ == ParserState::DcsEscape) {
        if (byte == '\\') {
            parserState_ = ParserState::Ground;
        } else if (byte != 0x1b) {
            parserState_ = ParserState::Dcs;
        }
        return;
    }

    if (byte == 0x1b) {
        parserState_ = ParserState::DcsEscape;
    }
}

void TerminalEmulator::beginCsi() {
    resetCsi();
    parserState_ = ParserState::CsiEntry;
}

void TerminalEmulator::resetCsi() {
    for (size_t index = 0; index < kTerminalMaxCsiParameters; ++index) {
        csiParameters_[index] = 0;
    }
    csiParameterCount_ = 0;
    csiDigitCount_ = 0;
    csiPrefix_ = 0;
    csiIntermediate_ = 0;
    csiParameterActive_ = false;
    csiMalformed_ = false;
}

void TerminalEmulator::appendCsiDigit(uint8_t digit) {
    if (!csiParameterActive_) {
        if (csiParameterCount_ >= kTerminalMaxCsiParameters) {
            csiMalformed_ = true;
            return;
        }
        csiParameters_[csiParameterCount_++] = 0;
        csiParameterActive_ = true;
        csiDigitCount_ = 0;
    }
    if (csiDigitCount_ >= kMaximumCsiDigits) {
        csiMalformed_ = true;
        return;
    }
    ++csiDigitCount_;
    uint32_t value = static_cast<uint32_t>(csiParameters_[csiParameterCount_ - 1]) * 10u +
                     digit;
    if (value > 65535u) value = 65535u;
    csiParameters_[csiParameterCount_ - 1] = static_cast<uint16_t>(value);
}

void TerminalEmulator::appendCsiSeparator() {
    if (!csiParameterActive_) {
        if (csiParameterCount_ >= kTerminalMaxCsiParameters) {
            csiMalformed_ = true;
            return;
        }
        csiParameters_[csiParameterCount_++] = 0;
    }
    csiParameterActive_ = false;
    csiDigitCount_ = 0;
}

uint16_t TerminalEmulator::csiParameter(size_t index, uint16_t defaultValue) const {
    if (index >= csiParameterCount_ || csiParameters_[index] == 0) return defaultValue;
    return csiParameters_[index];
}

uint8_t TerminalEmulator::csiCount(size_t index, uint8_t maximum) const {
    const uint16_t requested = csiParameter(index, 1);
    return requested > maximum ? maximum : static_cast<uint8_t>(requested);
}

uint8_t TerminalEmulator::csiCoordinate(size_t index, uint8_t maximum,
                                        uint16_t defaultValue) const {
    uint16_t coordinate = csiParameter(index, defaultValue);
    if (coordinate < 1) coordinate = 1;
    if (coordinate > maximum) coordinate = maximum;
    return static_cast<uint8_t>(coordinate - 1);
}

void TerminalEmulator::setCursor(int row, int column) {
    TerminalScreen& screen = activeScreenMutable();
    const TerminalCursor before = screen.cursor();
    screen.setCursor(row, column);
    if (before.row != screen.cursor().row || before.column != screen.cursor().column) {
        ++generation_;
    }
}

void TerminalEmulator::setStyle(uint8_t foreground, uint8_t background,
                                uint8_t attributes) {
    foreground_ = foreground;
    background_ = background;
    attributes_ = attributes;
    // Keep the inactive screen ready for a later switch. A future per-screen reset must
    // explicitly reapply this authoritative rendition.
    primary_.setStyle(foreground_, background_, attributes_);
    alternate_.setStyle(foreground_, background_, attributes_);
}

void TerminalEmulator::scrollUp(uint8_t count) {
    TerminalScreen& screen = activeScreenMutable();
    for (uint8_t index = 0; index < count; ++index) {
        TerminalRow removed{};
        const bool removedFullScreen = screen.scrollUp(1, &removed);
        if (removedFullScreen && !alternateActive_) history_.push(removed);
    }
    ++generation_;
}

void TerminalEmulator::setCursorVisible(bool visible) {
    if (cursorVisible_ == visible) return;
    cursorVisible_ = visible;
    primary_.setCursorVisible(visible);
    alternate_.setCursorVisible(visible);
    ++generation_;
}

void TerminalEmulator::setAlternateActive(bool active) {
    if (alternateActive_ == active) return;
    alternateActive_ = active;
    invalidatedRows_ = allRowsDirtyMask();
    ++generation_;
    if (!active) restorePrimary1049Cursor();
}

void TerminalEmulator::enterAlternate1049() {
    if (alternate1049Active_) return;
    savedPrimary1049Cursor_ = primary_.cursor();
    alternate1049Active_ = true;
    alternate_.reset();
    alternate_.setStyle(foreground_, background_, attributes_);
    alternate_.setCursorVisible(cursorVisible_);
    if (alternateActive_) {
        invalidatedRows_ = allRowsDirtyMask();
        ++generation_;
    } else {
        setAlternateActive(true);
    }
}

void TerminalEmulator::leaveAlternate1049() {
    if (alternateActive_) {
        setAlternateActive(false);
    } else {
        restorePrimary1049Cursor();
    }
}

void TerminalEmulator::restorePrimary1049Cursor() {
    if (!alternate1049Active_) return;
    alternate1049Active_ = false;
    TerminalScreen& screen = primary_;
    const TerminalCursor before = screen.cursor();
    const bool visiblyChanged =
        before.row != savedPrimary1049Cursor_.row ||
        before.column != savedPrimary1049Cursor_.column ||
        before.visible != savedPrimary1049Cursor_.visible;
    screen.setCursor(savedPrimary1049Cursor_.row, savedPrimary1049Cursor_.column);
    cursorVisible_ = savedPrimary1049Cursor_.visible;
    primary_.setCursorVisible(cursorVisible_);
    alternate_.setCursorVisible(cursorVisible_);
    if (visiblyChanged) ++generation_;
}

void TerminalEmulator::dispatchModes(bool privateModes, bool set) {
    for (size_t index = 0; index < csiParameterCount_; ++index) {
        const uint16_t mode = csiParameters_[index];
        if (!privateModes) {
            if (mode == 4) insertMode_ = set;
            continue;
        }

        switch (mode) {
            case 1:
                inputModes_.applicationCursor = set;
                break;
            case 7:
                autowrap_ = set;
                break;
            case 25:
                setCursorVisible(set);
                break;
            case 47:
            case 1047:
                setAlternateActive(set);
                break;
            case 1048:
                if (set) {
                    saveCursor();
                } else {
                    restoreCursor();
                }
                break;
            case 1049:
                if (set) {
                    enterAlternate1049();
                } else {
                    leaveAlternate1049();
                }
                break;
            default:
                break;
        }
    }
}

bool TerminalEmulator::enqueueReply(const TerminalReply& reply) {
    if (reply.length > kTerminalReplyBytes ||
        replyCount_ >= kTerminalReplyCapacity) {
        replyOverflowed_ = true;
        return false;
    }
    const uint8_t index =
        static_cast<uint8_t>((replyFirst_ + replyCount_) % kTerminalReplyCapacity);
    replies_[index] = reply;
    ++replyCount_;
    return true;
}

void TerminalEmulator::enqueuePrimaryDeviceAttributes() {
    TerminalReply reply;
    // VT100 with Advanced Video Option; fixed as the terminal's primary DA.
    const uint8_t bytes[] = {0x1b, '[', '?', '1', ';', '2', 'c'};
    for (uint8_t byte : bytes) appendReplyByte(reply, byte);
    enqueueReply(reply);
}

void TerminalEmulator::enqueueStatusReport() {
    TerminalReply reply;
    const uint8_t bytes[] = {0x1b, '[', '0', 'n'};
    for (uint8_t byte : bytes) appendReplyByte(reply, byte);
    enqueueReply(reply);
}

void TerminalEmulator::enqueueCursorReport() {
    TerminalReply reply;
    const TerminalCursor cursor = activeScreen().cursor();
    appendReplyByte(reply, 0x1b);
    appendReplyByte(reply, '[');
    appendReplyDecimal(reply, static_cast<uint8_t>(cursor.row + 1));
    appendReplyByte(reply, ';');
    appendReplyDecimal(reply, static_cast<uint8_t>(cursor.column + 1));
    appendReplyByte(reply, 'R');
    enqueueReply(reply);
}

void TerminalEmulator::dispatchCsi(uint8_t finalByte) {
    if (csiIntermediate_ != 0) return;
    if (finalByte == 'h' || finalByte == 'l') {
        if (csiPrefix_ == 0 || csiPrefix_ == '?') {
            dispatchModes(csiPrefix_ == '?', finalByte == 'h');
        }
        return;
    }
    if (csiPrefix_ != 0) return;

    TerminalScreen& screen = activeScreenMutable();
    const TerminalCursor cursor = screen.cursor();
    switch (finalByte) {
        case 'A':
            setCursor(static_cast<int>(cursor.row) - csiCount(0, kTerminalRows),
                      cursor.column);
            break;
        case 'B':
            setCursor(static_cast<int>(cursor.row) + csiCount(0, kTerminalRows),
                      cursor.column);
            break;
        case 'C':
            setCursor(cursor.row,
                      static_cast<int>(cursor.column) + csiCount(0, kTerminalColumns));
            break;
        case 'D':
            setCursor(cursor.row,
                      static_cast<int>(cursor.column) - csiCount(0, kTerminalColumns));
            break;
        case 'E':
            setCursor(static_cast<int>(cursor.row) + csiCount(0, kTerminalRows), 0);
            break;
        case 'F':
            setCursor(static_cast<int>(cursor.row) - csiCount(0, kTerminalRows), 0);
            break;
        case 'G':
            setCursor(cursor.row, csiCoordinate(0, kTerminalColumns));
            break;
        case 'H':
        case 'f':
            setCursor(csiCoordinate(0, kTerminalRows),
                      csiCoordinate(1, kTerminalColumns));
            break;
        case 'd':
            setCursor(csiCoordinate(0, kTerminalRows), cursor.column);
            break;
        case 'J': {
            const uint16_t mode = csiParameterCount_ == 0 ? 0 : csiParameters_[0];
            if (mode == 0) {
                screen.eraseDisplay(TerminalEraseMode::After);
                ++generation_;
            } else if (mode == 1) {
                screen.eraseDisplay(TerminalEraseMode::Before);
                ++generation_;
            } else if (mode == 2) {
                screen.eraseDisplay(TerminalEraseMode::All);
                ++generation_;
            } else if (mode == 3) {
                history_.clear();
            }
            break;
        }
        case 'K': {
            const uint16_t mode = csiParameterCount_ == 0 ? 0 : csiParameters_[0];
            if (mode == 0) {
                screen.eraseLine(TerminalEraseMode::After);
                ++generation_;
            } else if (mode == 1) {
                screen.eraseLine(TerminalEraseMode::Before);
                ++generation_;
            } else if (mode == 2) {
                screen.eraseLine(TerminalEraseMode::All);
                ++generation_;
            }
            break;
        }
        case '@':
            screen.insertCharacters(csiCount(0, kTerminalColumns));
            ++generation_;
            break;
        case 'P':
            screen.deleteCharacters(csiCount(0, kTerminalColumns));
            ++generation_;
            break;
        case 'L':
            if (cursor.row >= screen.marginTop() && cursor.row <= screen.marginBottom()) {
                screen.insertLines(csiCount(0, kTerminalRows));
                ++generation_;
            }
            break;
        case 'M':
            if (cursor.row >= screen.marginTop() && cursor.row <= screen.marginBottom()) {
                screen.deleteLines(csiCount(0, kTerminalRows));
                ++generation_;
            }
            break;
        case 'S': {
            const uint8_t regionHeight =
                static_cast<uint8_t>(screen.marginBottom() - screen.marginTop() + 1);
            scrollUp(csiCount(0, regionHeight));
            break;
        }
        case 'T': {
            const uint8_t regionHeight =
                static_cast<uint8_t>(screen.marginBottom() - screen.marginTop() + 1);
            screen.scrollDown(csiCount(0, regionHeight));
            ++generation_;
            break;
        }
        case 'm':
            dispatchSgr();
            break;
        case 'c':
            if (csiParameterCount_ == 0 ||
                (csiParameterCount_ == 1 && csiParameters_[0] == 0)) {
                enqueuePrimaryDeviceAttributes();
            }
            break;
        case 'n':
            if (csiParameterCount_ == 1 && csiParameters_[0] == 5) {
                enqueueStatusReport();
            } else if (csiParameterCount_ == 1 && csiParameters_[0] == 6) {
                enqueueCursorReport();
            }
            break;
        case 'r': {
            const uint8_t top = csiCoordinate(0, kTerminalRows, 1);
            const uint8_t bottom = csiCoordinate(1, kTerminalRows, kTerminalRows);
            if (top < bottom) {
                screen.setMargins(top, bottom);
                setCursor(0, 0);
            }
            break;
        }
        default:
            break;
    }
}

void TerminalEmulator::dispatchSgr() {
    uint8_t foreground = foreground_;
    uint8_t background = background_;
    uint8_t attributes = attributes_;

    if (csiParameterCount_ == 0) {
        foreground = 7;
        background = 0;
        attributes = 0;
    }

    for (size_t index = 0; index < csiParameterCount_; ++index) {
        const uint16_t code = csiParameters_[index];
        if (code == 38 || code == 48) {
            if (index + 1 >= csiParameterCount_) return;
            const uint16_t selector = csiParameters_[index + 1];
            if (selector == 5) {
                if (index + 2 >= csiParameterCount_) return;
                const uint8_t color = clippedByte(csiParameters_[index + 2]);
                if (code == 38) {
                    foreground = color;
                } else {
                    background = color;
                }
                index += 2;
            } else if (selector == 2) {
                if (index + 4 >= csiParameterCount_) return;
                const uint8_t red = clippedByte(csiParameters_[index + 2]);
                const uint8_t green = clippedByte(csiParameters_[index + 3]);
                const uint8_t blue = clippedByte(csiParameters_[index + 4]);
                const uint8_t color = nearestXtermColor(red, green, blue);
                if (code == 38) {
                    foreground = color;
                } else {
                    background = color;
                }
                index += 4;
            } else {
                return;
            }
        } else if (code == 0) {
            foreground = 7;
            background = 0;
            attributes = 0;
        } else if (code == 1) {
            attributes |= kTerminalBold;
        } else if (code == 4) {
            attributes |= kTerminalUnderline;
        } else if (code == 7) {
            attributes |= kTerminalInverse;
        } else if (code == 22) {
            attributes &= static_cast<uint8_t>(~kTerminalBold);
        } else if (code == 24) {
            attributes &= static_cast<uint8_t>(~kTerminalUnderline);
        } else if (code == 27) {
            attributes &= static_cast<uint8_t>(~kTerminalInverse);
        } else if (code >= 30 && code <= 37) {
            foreground = static_cast<uint8_t>(code - 30);
        } else if (code == 39) {
            foreground = 7;
        } else if (code >= 40 && code <= 47) {
            background = static_cast<uint8_t>(code - 40);
        } else if (code == 49) {
            background = 0;
        } else if (code >= 90 && code <= 97) {
            foreground = static_cast<uint8_t>(code - 90 + 8);
        } else if (code >= 100 && code <= 107) {
            background = static_cast<uint8_t>(code - 100 + 8);
        }
    }

    setStyle(foreground, background, attributes);
}

bool TerminalEmulator::executeControl(uint8_t byte) {
    TerminalScreen& screen = activeScreenMutable();
    switch (byte) {
        case 0x07:  // BEL
        case 0x7f:  // DEL
            return false;
        case 0x08:  // BS
            if (screen.cursor().column == 0) return false;
            screen.backspace();
            ++generation_;
            return true;
        case 0x09: {  // HT
            const uint8_t before = screen.cursor().column;
            screen.tab();
            if (screen.cursor().column == before) return false;
            ++generation_;
            return true;
        }
        case 0x0a:  // LF
        case 0x0b:  // VT
        case 0x0c:  // FF
            lineFeed();
            return true;
        case 0x0d:  // CR
            if (screen.cursor().column == 0) return false;
            screen.carriageReturn();
            ++generation_;
            return true;
        default:
            return false;
    }
}

void TerminalEmulator::putCodePoint(uint32_t codePoint) {
    TerminalRow removed{};
    const uint16_t glyph =
        decGraphics_ ? decSpecialGraphicsGlyph(codePoint)
                     : normalizeTerminalCodePoint(codePoint);
    const bool removedFullScreen =
        activeScreenMutable().putGlyph(glyph, insertMode_, autowrap_, removed);
    if (removedFullScreen && !alternateActive_) history_.push(removed);
    ++generation_;
}

void TerminalEmulator::lineFeed() {
    TerminalRow removed{};
    const bool removedFullScreen = activeScreenMutable().lineFeed(removed);
    if (removedFullScreen && !alternateActive_) history_.push(removed);
    ++generation_;
}

void TerminalEmulator::nextLine() {
    TerminalScreen& screen = activeScreenMutable();
    if (screen.cursor().column != 0) screen.carriageReturn();
    lineFeed();
}

void TerminalEmulator::reverseIndex() {
    if (activeScreenMutable().reverseIndex()) ++generation_;
}

void TerminalEmulator::saveCursor() {
    if (alternateActive_) {
        savedAlternateCursor_ = alternate_.cursor();
    } else {
        savedPrimaryCursor_ = primary_.cursor();
    }
}

void TerminalEmulator::restoreCursor() {
    TerminalScreen& screen = activeScreenMutable();
    const TerminalCursor& saved =
        alternateActive_ ? savedAlternateCursor_ : savedPrimaryCursor_;
    const TerminalCursor before = screen.cursor();
    const bool visiblyChanged = before.row != saved.row || before.column != saved.column ||
                                before.visible != saved.visible;
    screen.setCursor(saved.row, saved.column);
    cursorVisible_ = saved.visible;
    primary_.setCursorVisible(cursorVisible_);
    alternate_.setCursorVisible(cursorVisible_);
    if (visiblyChanged) ++generation_;
}

void TerminalEmulator::resetParser() {
    parserState_ = ParserState::Ground;
    escapeIntermediate_ = 0;
    resetCsi();
    resetUtf8();
}

void TerminalEmulator::resetUtf8() {
    utf8CodePoint_ = 0;
    utf8Minimum_ = 0;
    utf8Remaining_ = 0;
    utf8Error_ = false;
}

}  // namespace app
