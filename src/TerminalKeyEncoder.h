#pragma once

#include <stddef.h>
#include <stdint.h>

#include "KeyboardRouter.h"
#include "TerminalTypes.h"

namespace app {

struct EncodedKey {
    uint8_t bytes[5]{};
    uint8_t length = 0;
};

class TerminalKeyEncoder {
public:
    static EncodedKey encode(const KeyEvent& event,
                             const TerminalInputModes& modes = {});
};

}  // namespace app
