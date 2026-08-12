#include "TerminalKeyEncoder.h"

namespace app {

namespace {

EncodedKey one(uint8_t value) {
    EncodedKey encoded;
    encoded.bytes[0] = value;
    encoded.length = 1;
    return encoded;
}

EncodedKey functionKey(uint8_t number) {
    if (number >= 1 && number <= 4) {
        EncodedKey encoded;
        encoded.bytes[0] = 0x1b;
        encoded.bytes[1] = 'O';
        encoded.bytes[2] = static_cast<uint8_t>('P' + number - 1);
        encoded.length = 3;
        return encoded;
    }
    constexpr uint8_t kTildeNumbers[] = {15, 17, 18, 19, 20, 21, 23, 24};
    if (number < 5 || number > 12) {
        return {};
    }
    const uint8_t value = kTildeNumbers[number - 5];
    EncodedKey encoded;
    encoded.bytes[0] = 0x1b;
    encoded.bytes[1] = '[';
    encoded.bytes[2] = static_cast<uint8_t>('0' + value / 10);
    encoded.bytes[3] = static_cast<uint8_t>('0' + value % 10);
    encoded.bytes[4] = '~';
    encoded.length = 5;
    return encoded;
}

EncodedKey cursorSequence(uint8_t finalByte,
                          const TerminalInputModes& modes) {
    EncodedKey encoded;
    encoded.bytes[0] = 0x1b;
    encoded.bytes[1] = modes.applicationCursor ? 'O' : '[';
    encoded.bytes[2] = finalByte;
    encoded.length = 3;
    return encoded;
}

}  // namespace

EncodedKey TerminalKeyEncoder::encode(const KeyEvent& event,
                                      const TerminalInputModes& modes) {
    switch (event.type) {
        case KeyEventType::Text:
        case KeyEventType::Control:
            return one(event.value);
        case KeyEventType::AltText: {
            EncodedKey encoded;
            encoded.bytes[0] = 0x1b;
            encoded.bytes[1] = event.value;
            encoded.length = 2;
            return encoded;
        }
        case KeyEventType::Function:
            return functionKey(event.value);
        case KeyEventType::Confirm:
            return one(0x0d);
        case KeyEventType::Tab:
            return one(0x09);
        case KeyEventType::DeleteBackward:
            return one(0x7f);
        case KeyEventType::DeleteForward: {
            EncodedKey encoded;
            encoded.bytes[0] = 0x1b;
            encoded.bytes[1] = '[';
            encoded.bytes[2] = '3';
            encoded.bytes[3] = '~';
            encoded.length = 4;
            return encoded;
        }
        case KeyEventType::MoveUp:
            return cursorSequence('A', modes);
        case KeyEventType::MoveDown:
            return cursorSequence('B', modes);
        case KeyEventType::MoveRight:
            return cursorSequence('C', modes);
        case KeyEventType::MoveLeft:
            return cursorSequence('D', modes);
        case KeyEventType::Escape:
            return one(0x1b);
        default:
            return {};
    }
}

}  // namespace app
