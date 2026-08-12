#pragma once

#include <stdint.h>

namespace app {

uint16_t xtermColorRgb565(uint8_t index);
uint8_t nearestXtermColor(uint8_t red, uint8_t green, uint8_t blue);

}  // namespace app
