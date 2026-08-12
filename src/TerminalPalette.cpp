#include "TerminalPalette.h"

#include <stdint.h>

namespace app {
namespace {

struct RgbColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

constexpr RgbColor kAnsiColors[16] = {
    {0x00, 0x00, 0x00}, {0x80, 0x00, 0x00}, {0x00, 0x80, 0x00},
    {0x80, 0x80, 0x00}, {0x00, 0x00, 0x80}, {0x80, 0x00, 0x80},
    {0x00, 0x80, 0x80}, {0xc0, 0xc0, 0xc0}, {0x80, 0x80, 0x80},
    {0xff, 0x55, 0x55}, {0x55, 0xff, 0x55}, {0xff, 0xff, 0x55},
    {0x55, 0x55, 0xff}, {0xff, 0x55, 0xff}, {0x55, 0xff, 0xff},
    {0xff, 0xff, 0xff},
};

constexpr uint8_t kCubeLevels[6] = {0, 95, 135, 175, 215, 255};

RgbColor xtermColor(uint8_t index) {
    if (index < 16) return kAnsiColors[index];
    if (index < 232) {
        const uint8_t cubeIndex = static_cast<uint8_t>(index - 16);
        const uint8_t redIndex = static_cast<uint8_t>(cubeIndex / 36);
        const uint8_t greenIndex = static_cast<uint8_t>((cubeIndex / 6) % 6);
        const uint8_t blueIndex = static_cast<uint8_t>(cubeIndex % 6);
        return {kCubeLevels[redIndex], kCubeLevels[greenIndex], kCubeLevels[blueIndex]};
    }
    const uint8_t level = static_cast<uint8_t>(8 + 10 * (index - 232));
    return {level, level, level};
}

uint16_t rgb565(const RgbColor& color) {
    return static_cast<uint16_t>((static_cast<uint16_t>(color.red & 0xf8) << 8) |
                                 (static_cast<uint16_t>(color.green & 0xfc) << 3) |
                                 (color.blue >> 3));
}

uint32_t squaredDistance(uint8_t red, uint8_t green, uint8_t blue,
                         const RgbColor& candidate) {
    const int32_t redDelta = static_cast<int32_t>(red) - candidate.red;
    const int32_t greenDelta = static_cast<int32_t>(green) - candidate.green;
    const int32_t blueDelta = static_cast<int32_t>(blue) - candidate.blue;
    return static_cast<uint32_t>(redDelta * redDelta + greenDelta * greenDelta +
                                 blueDelta * blueDelta);
}

}  // namespace

uint16_t xtermColorRgb565(uint8_t index) {
    return rgb565(xtermColor(index));
}

uint8_t nearestXtermColor(uint8_t red, uint8_t green, uint8_t blue) {
    uint8_t nearest = 0;
    uint32_t nearestDistance = squaredDistance(red, green, blue, xtermColor(0));
    for (uint16_t index = 1; index < 256; ++index) {
        const uint32_t distance =
            squaredDistance(red, green, blue, xtermColor(static_cast<uint8_t>(index)));
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = static_cast<uint8_t>(index);
        }
    }
    return nearest;
}

}  // namespace app
