#include "UiRenderer.h"

#include <string.h>

#include "TerminalGlyphScale.h"
#include "TerminalPalette.h"

namespace app {

namespace {

constexpr uint16_t kBackground = 0x0841;
constexpr uint16_t kForeground = 0xFFFF;
constexpr uint16_t kMuted = 0x8410;
constexpr uint16_t kAccent = 0x07FF;
constexpr uint16_t kPanel = 0x1082;
constexpr uint16_t kTerminalBackground = 0x0000;
constexpr uint16_t kAllTerminalRows =
    static_cast<uint16_t>((1U << kTerminalRows) - 1U);

// Two bits per direction, ordered up/right/down/left. Values are none,
// light, heavy, and double. The table preserves the glyph-specific mixed
// weights across the complete Box Drawing block.
constexpr uint8_t kBoxConnections[128] = {
    0x44, 0x88, 0x11, 0x22, 0x44, 0x88, 0x11, 0x22,
    0x44, 0x88, 0x11, 0x22, 0x14, 0x18, 0x24, 0x28,
    0x50, 0x90, 0x60, 0xa0, 0x05, 0x09, 0x06, 0x0a,
    0x41, 0x81, 0x42, 0x82, 0x15, 0x19, 0x16, 0x25,
    0x26, 0x1a, 0x29, 0x2a, 0x51, 0x91, 0x52, 0x61,
    0x62, 0x92, 0xa1, 0xa2, 0x54, 0x94, 0x58, 0x98,
    0x64, 0xa4, 0x68, 0xa8, 0x45, 0x85, 0x49, 0x89,
    0x46, 0x86, 0x4a, 0x8a, 0x55, 0x95, 0x59, 0x99,
    0x56, 0x65, 0x66, 0x96, 0x5a, 0xa5, 0x69, 0x9a,
    0xa9, 0xa6, 0x6a, 0xaa, 0x44, 0x88, 0x11, 0x22,
    0xcc, 0x33, 0x1c, 0x34, 0x3c, 0xd0, 0x70, 0xf0,
    0x0d, 0x07, 0x0f, 0xc1, 0x43, 0xc3, 0x1d, 0x37,
    0x3f, 0xd1, 0x73, 0xf3, 0xdc, 0x74, 0xfc, 0xcd,
    0x47, 0xcf, 0xdd, 0x77, 0xff, 0x14, 0x50, 0x41,
    0x05, 0x00, 0x00, 0x00, 0x40, 0x01, 0x04, 0x10,
    0x80, 0x02, 0x08, 0x20, 0x48, 0x21, 0x84, 0x12,
};

void drawGlyphPixel(M5Canvas& canvas, int x, int y, int cellX, int cellY,
                    uint16_t color, bool bold) {
    if (x < 0 || y < 0) return;
    const TerminalGlyphSpan horizontal = scaleTerminalGlyphSpan(
        static_cast<uint8_t>(x), 1, kTerminalGlyphSourceWidth,
        kTerminalCellWidth);
    const TerminalGlyphSpan vertical = scaleTerminalGlyphSpan(
        static_cast<uint8_t>(y), 1, kTerminalGlyphSourceHeight,
        kTerminalCellHeight);
    if (horizontal.length == 0 || vertical.length == 0) return;

    canvas.fillRect(cellX + horizontal.offset, cellY + vertical.offset,
                    horizontal.length, vertical.length, color);
    const uint8_t after = horizontal.offset + horizontal.length;
    if (bold && after < kTerminalCellWidth) {
        canvas.fillRect(cellX + after, cellY + vertical.offset, 1,
                        vertical.length, color);
    }
}

void drawMaskGlyph(M5Canvas& canvas, const uint8_t columns[5], int cellX,
                   int cellY, uint16_t color, bool bold) {
    for (int x = 0; x < kTerminalGlyphSourceWidth; ++x) {
        for (int y = 0; y < kTerminalGlyphSourceHeight; ++y) {
            if ((columns[x] & (1U << y)) != 0) {
                drawGlyphPixel(canvas, x, y, cellX, cellY, color, bold);
            }
        }
    }
}

void drawHorizontalSegment(M5Canvas& canvas, uint8_t style, int first,
                           int last, int cellX, int cellY, uint16_t color,
                           bool bold) {
    const uint8_t rows[][2] = {{3, 3}, {3, 3}, {3, 4}, {2, 5}};
    if (style == 0 || style > 3) return;
    drawGlyphPixel(canvas, first, rows[style][0], cellX, cellY, color, bold);
    for (int x = first + 1; x <= last; ++x) {
        drawGlyphPixel(canvas, x, rows[style][0], cellX, cellY, color, bold);
    }
    if (rows[style][1] != rows[style][0]) {
        for (int x = first; x <= last; ++x) {
            drawGlyphPixel(canvas, x, rows[style][1], cellX, cellY, color,
                           bold);
        }
    }
}

void drawVerticalSegment(M5Canvas& canvas, uint8_t style, int first, int last,
                         int cellX, int cellY, uint16_t color, bool bold) {
    const uint8_t columns[][2] = {{2, 2}, {2, 2}, {2, 3}, {1, 3}};
    if (style == 0 || style > 3) return;
    for (int y = first; y <= last; ++y) {
        drawGlyphPixel(canvas, columns[style][0], y, cellX, cellY, color,
                       bold);
    }
    if (columns[style][1] != columns[style][0]) {
        for (int y = first; y <= last; ++y) {
            drawGlyphPixel(canvas, columns[style][1], y, cellX, cellY, color,
                           bold);
        }
    }
}

void drawDashedBoxGlyph(M5Canvas& canvas, uint16_t glyph, uint8_t style,
                        bool vertical, int cellX, int cellY, uint16_t color,
                        bool bold) {
    const uint8_t dashCount =
        glyph >= 0x254c ? 2 : (glyph >= 0x2508 ? 4 : 3);
    if (!vertical) {
        const uint8_t patterns[][5] = {
            {0, 1, 0, 0, 0}, {0, 1, 3, 4, 0}, {0, 2, 4, 0, 0}};
        const uint8_t* points = dashCount == 2 ? patterns[1]
                                : dashCount == 3 ? patterns[2]
                                                 : patterns[1];
        const uint8_t pointCount = dashCount == 3 ? 3 : 4;
        for (uint8_t index = 0; index < pointCount; ++index) {
            drawHorizontalSegment(canvas, style, points[index], points[index],
                                  cellX, cellY, color, bold);
        }
        return;
    }

    const uint8_t doublePoints[] = {0, 1, 6, 7};
    const uint8_t triplePoints[] = {0, 3, 7};
    const uint8_t quadPoints[] = {0, 2, 5, 7};
    const uint8_t* points = dashCount == 2 ? doublePoints
                            : dashCount == 3 ? triplePoints
                                             : quadPoints;
    const uint8_t pointCount = dashCount == 3 ? 3 : 4;
    for (uint8_t index = 0; index < pointCount; ++index) {
        drawVerticalSegment(canvas, style, points[index], points[index], cellX,
                            cellY, color, bold);
    }
}

void drawRoundedBoxGlyph(M5Canvas& canvas, uint16_t glyph, int cellX,
                         int cellY, uint16_t color, bool bold) {
    uint8_t columns[5] = {};
    if (terminalRoundedBoxMask(glyph, columns)) {
        drawMaskGlyph(canvas, columns, cellX, cellY, color, bold);
    }
}

void drawBoxGlyph(M5Canvas& canvas, uint16_t glyph, int cellX, int cellY,
                  uint16_t color, bool bold) {
    if (glyph < 0x2500 || glyph > 0x257f) return;
    if (glyph >= 0x2571 && glyph <= 0x2573) {
        for (int y = 0; y < kTerminalGlyphSourceHeight; ++y) {
            const int slashX = 4 - (y * 5) / 8;
            const int backslashX = (y * 5) / 8;
            if (glyph != 0x2572) {
                drawGlyphPixel(canvas, slashX, y, cellX, cellY, color, bold);
            }
            if (glyph != 0x2571) {
                drawGlyphPixel(canvas, backslashX, y, cellX, cellY, color,
                               bold);
            }
        }
        return;
    }
    if (glyph >= 0x256d && glyph <= 0x2570) {
        drawRoundedBoxGlyph(canvas, glyph, cellX, cellY, color, bold);
        return;
    }

    const uint8_t directions = kBoxConnections[glyph - 0x2500];
    const uint8_t up = directions & 0x03;
    const uint8_t right = (directions >> 2) & 0x03;
    const uint8_t down = (directions >> 4) & 0x03;
    const uint8_t left = (directions >> 6) & 0x03;
    const bool dashed = (glyph >= 0x2504 && glyph <= 0x250b) ||
                        (glyph >= 0x254c && glyph <= 0x254f);
    if (dashed) {
        const bool vertical = glyph == 0x2506 || glyph == 0x2507 ||
                              glyph == 0x250a || glyph == 0x250b ||
                              glyph == 0x254e || glyph == 0x254f;
        drawDashedBoxGlyph(canvas, glyph, vertical ? (up ? up : down)
                                                   : (left ? left : right),
                           vertical, cellX, cellY, color, bold);
        return;
    }
    drawVerticalSegment(canvas, up, 0, 3, cellX, cellY, color, bold);
    drawHorizontalSegment(canvas, right, 2, 4, cellX, cellY, color, bold);
    drawVerticalSegment(canvas, down, 3, 7, cellX, cellY, color, bold);
    drawHorizontalSegment(canvas, left, 0, 2, cellX, cellY, color, bold);
}

void fillBlockPart(M5Canvas& canvas, int x, int y, int width, int height,
                   int cellX, int cellY, uint16_t color, bool bold) {
    if (x < 0 || y < 0 || width <= 0 || height <= 0) return;
    const TerminalGlyphSpan horizontal = scaleTerminalGlyphSpan(
        static_cast<uint8_t>(x), static_cast<uint8_t>(width),
        kTerminalGlyphSourceWidth, kTerminalCellWidth);
    const TerminalGlyphSpan vertical = scaleTerminalGlyphSpan(
        static_cast<uint8_t>(y), static_cast<uint8_t>(height),
        kTerminalGlyphSourceHeight, kTerminalCellHeight);
    if (horizontal.length == 0 || vertical.length == 0) return;

    canvas.fillRect(cellX + horizontal.offset, cellY + vertical.offset,
                    horizontal.length, vertical.length, color);
    const uint8_t after = horizontal.offset + horizontal.length;
    if (bold && after < kTerminalCellWidth) {
        canvas.fillRect(cellX + after, cellY + vertical.offset, 1,
                        vertical.length, color);
    }
}

void drawBlockGlyph(M5Canvas& canvas, uint16_t glyph, int cellX, int cellY,
                    uint16_t color, bool bold) {
    if (glyph == 0x2580) {
        fillBlockPart(canvas, 0, 0, 5, 4, cellX, cellY, color, bold);
    } else if (glyph >= 0x2581 && glyph <= 0x2588) {
        const int height = glyph - 0x2580;
        fillBlockPart(canvas, 0, 8 - height, 5, height, cellX, cellY, color,
                      bold);
    } else if (glyph >= 0x2589 && glyph <= 0x258f) {
        const int eighths = 0x2590 - glyph;
        const int width = (5 * eighths + 4) / 8;
        fillBlockPart(canvas, 0, 0, width, 8, cellX, cellY, color, bold);
    } else if (glyph == 0x2590) {
        fillBlockPart(canvas, 2, 0, 3, 8, cellX, cellY, color, bold);
    } else if (glyph >= 0x2591 && glyph <= 0x2593) {
        const uint8_t threshold = static_cast<uint8_t>(glyph - 0x2590);
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 5; ++x) {
                if (((x + y * 2) & 3) < threshold) {
                    drawGlyphPixel(canvas, x, y, cellX, cellY, color, bold);
                }
            }
        }
    } else if (glyph == 0x2594) {
        fillBlockPart(canvas, 0, 0, 5, 1, cellX, cellY, color, bold);
    } else if (glyph == 0x2595) {
        fillBlockPart(canvas, 4, 0, 1, 8, cellX, cellY, color, bold);
    } else {
        const uint8_t quadrants = static_cast<uint8_t>(glyph - 0x2596);
        constexpr uint8_t masks[] = {
            0x04, 0x08, 0x01, 0x0d, 0x09,
            0x07, 0x0b, 0x02, 0x06, 0x0e,
        };
        if (quadrants >= sizeof(masks)) return;
        const uint8_t mask = masks[quadrants];
        if ((mask & 0x01) != 0) {
            fillBlockPart(canvas, 0, 0, 2, 4, cellX, cellY, color, bold);
        }
        if ((mask & 0x02) != 0) {
            fillBlockPart(canvas, 2, 0, 3, 4, cellX, cellY, color, bold);
        }
        if ((mask & 0x04) != 0) {
            fillBlockPart(canvas, 0, 4, 2, 4, cellX, cellY, color, bold);
        }
        if ((mask & 0x08) != 0) {
            fillBlockPart(canvas, 2, 4, 3, 4, cellX, cellY, color, bold);
        }
    }
}

void drawBrailleGlyph(M5Canvas& canvas, uint16_t glyph, int cellX, int cellY,
                      uint16_t color, bool bold) {
    const uint8_t dots = static_cast<uint8_t>(glyph - 0x2800);
    constexpr uint8_t bitAt[4][2] = {
        {0, 3}, {1, 4}, {2, 5}, {6, 7},
    };
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 2; ++column) {
            if ((dots & (1U << bitAt[row][column])) != 0) {
                drawGlyphPixel(canvas, column == 0 ? 1 : 3, row * 2, cellX,
                               cellY, color, bold);
            }
        }
    }
}

void drawTerminalCell(M5Canvas& canvas, const TerminalCell& cell, int cellX,
                      int cellY, bool cursor) {
    uint8_t foregroundIndex = cell.foreground();
    uint8_t backgroundIndex = cell.background();
    if ((cell.attributes() & kTerminalInverse) != 0) {
        const uint8_t swap = foregroundIndex;
        foregroundIndex = backgroundIndex;
        backgroundIndex = swap;
    }
    if (cursor) {
        const uint8_t swap = foregroundIndex;
        foregroundIndex = backgroundIndex;
        backgroundIndex = swap;
    }
    const uint16_t foreground = xtermColorRgb565(foregroundIndex);
    const uint16_t background = xtermColorRgb565(backgroundIndex);
    const bool bold = (cell.attributes() & kTerminalBold) != 0;
    canvas.fillRect(cellX, cellY, kTerminalCellWidth, kTerminalCellHeight,
                    background);
    canvas.setClipRect(cellX, cellY, kTerminalCellWidth, kTerminalCellHeight);

    const uint16_t glyph = cell.glyph();
    switch (terminalGlyphKind(glyph)) {
        case TerminalGlyphKind::Builtin:
            canvas.setFont(&fonts::Font0);
            canvas.setTextSize(
                static_cast<float>(kTerminalCellWidth) /
                    kTerminalGlyphSourceWidth,
                static_cast<float>(kTerminalCellHeight) /
                    kTerminalGlyphSourceHeight);
            canvas.setTextDatum(top_left);
            canvas.setTextColor(foreground);
            canvas.drawChar(glyph, cellX, cellY);
            if (bold) canvas.drawChar(glyph, cellX + 1, cellY);
            break;
        case TerminalGlyphKind::BoxDrawing:
            drawBoxGlyph(canvas, glyph, cellX, cellY, foreground, bold);
            break;
        case TerminalGlyphKind::Block:
            drawBlockGlyph(canvas, glyph, cellX, cellY, foreground, bold);
            break;
        case TerminalGlyphKind::Braille:
            drawBrailleGlyph(canvas, glyph, cellX, cellY, foreground, bold);
            break;
        default: {
            uint8_t columns[5]{};
            if (terminalGlyphMask(glyph, columns)) {
                drawMaskGlyph(canvas, columns, cellX, cellY, foreground, bold);
            }
            break;
        }
    }
    if ((cell.attributes() & kTerminalUnderline) != 0) {
        canvas.drawFastHLine(cellX, cellY + kTerminalCellHeight - 1,
                             kTerminalCellWidth, foreground);
    }
    canvas.clearClipRect();
}

void drawCompactLine(M5Canvas& canvas, const char* text, int y,
                     uint16_t foreground, uint16_t background) {
    canvas.fillRect(kTerminalBodyX, y,
                    kTerminalColumns * kTerminalCellWidth,
                    kTerminalCellHeight, background);
    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(
        static_cast<float>(kTerminalCellWidth) / kTerminalGlyphSourceWidth,
        static_cast<float>(kTerminalCellHeight) / kTerminalGlyphSourceHeight);
    canvas.setTextDatum(top_left);
    for (size_t index = 0; text[index] != '\0' && index < kTerminalColumns;
         ++index) {
        const int x = terminalCellX(index);
        canvas.setClipRect(x, y, kTerminalCellWidth, kTerminalCellHeight);
        canvas.setTextColor(foreground);
        canvas.drawChar(static_cast<uint8_t>(text[index]), x, y);
    }
    canvas.clearClipRect();
}

}  // namespace

UiRenderer::UiRenderer() : canvas_(&M5Cardputer.Display) {}

bool UiRenderer::begin() {
    canvas_.setColorDepth(8);
    if (canvas_.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height()) == nullptr) {
        return false;
    }
    canvas_.setTextSize(1);
    canvas_.setTextWrap(false);
    canvas_.setFont(&fonts::Font0);
    canvas_.fillSprite(kBackground);
    canvas_.pushSprite(0, 0);
    terminalFrameValid_ = false;
    return true;
}

void UiRenderer::progress(const String& title,
                          const String& detail,
                          uint16_t current,
                          uint16_t total,
                          bool force) {
    if (!force && millis() - lastProgressDraw_ < 80U) {
        return;
    }
    lastProgressDraw_ = millis();
    frame("CARDPUTER SSH", "WORKING", kAccent);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(kAccent, kBackground);
    canvas_.drawString(clipped(title, 36), canvas_.width() / 2, 50);
    canvas_.setTextColor(kForeground, kBackground);
    canvas_.drawString(clipped(detail, 38), canvas_.width() / 2, 70);

    if (total > 0) {
        const int width = 190;
        const int filled = min(width, static_cast<int>((static_cast<uint32_t>(current) * width) / total));
        canvas_.drawRect(25, 91, width, 8, kMuted);
        canvas_.fillRect(26, 92, max(0, filled - 2), 6, kAccent);
    } else {
        String dots;
        const uint8_t dotCount = static_cast<uint8_t>((millis() / 300U) % 4U);
        for (uint8_t index = 0; index < dotCount; ++index) {
            dots += '.';
        }
        canvas_.drawString(dots, canvas_.width() / 2, 92);
    }
    canvas_.setTextColor(kMuted, kBackground);
    canvas_.drawString("Fn+Esc: cancel", canvas_.width() / 2, 116);
    canvas_.pushSprite(0, 0);
}

void UiRenderer::list(const String& title,
                      const String* labels,
                      const String* details,
                      size_t count,
                      size_t selected,
                      size_t first,
                      const String& footer,
                      const String& step) {
    frame("CARDPUTER SSH", step, kAccent);
    canvas_.setTextDatum(top_left);
    canvas_.setTextColor(kForeground, kBackground);
    canvas_.drawString(clipped(title, 36), 4, 18);

    for (size_t row = 0; row < 3 && first + row < count; ++row) {
        const size_t index = first + row;
        const int y = 34 + static_cast<int>(row) * 18;
        const bool active = index == selected;
        if (active) {
            canvas_.fillRoundRect(0, y - 2, canvas_.width(), 17, 3, kAccent);
        }
        canvas_.setTextColor(active ? kBackground : kForeground, active ? kAccent : kBackground);
        canvas_.drawString((active ? "> " : "  ") + clipped(labels[index], 35), 4, y + 1);
    }
    if (count > 0 && selected < count && details != nullptr && !details[selected].isEmpty()) {
        canvas_.setTextColor(kMuted, kBackground);
        canvas_.drawString(clipped(details[selected], 38), 5, 91);
    }

    canvas_.fillRect(0, 119, canvas_.width(), 16, kPanel);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(kMuted, kPanel);
    canvas_.drawString(clipped(footer, 38), canvas_.width() / 2, 127);
    canvas_.pushSprite(0, 0);
}

void UiRenderer::input(const String& title,
                       const String& label,
                       const String& visibleValue,
                       size_t cursorColumn,
                       bool clippedLeft,
                       const String& footer,
                       const String& step) {
    frame("CARDPUTER SSH", step, kAccent);
    canvas_.setTextDatum(top_left);
    canvas_.setTextColor(kForeground, kBackground);
    canvas_.drawString(clipped(title, 36), 5, 22);
    canvas_.setTextColor(kMuted, kBackground);
    canvas_.drawString(clipped(label, 36), 5, 45);

    canvas_.fillRoundRect(4, 62, canvas_.width() - 8, 24, 3, kPanel);
    canvas_.setTextColor(kForeground, kPanel);
    const int textX = clippedLeft ? 14 : 8;
    if (clippedLeft) {
        canvas_.setTextColor(kMuted, kPanel);
        canvas_.drawString("<", 7, 70);
        canvas_.setTextColor(kForeground, kPanel);
    }
    canvas_.drawString(visibleValue, textX, 70);
    const int cursorX = min(canvas_.width() - 7, textX + static_cast<int>(cursorColumn) * 6);
    canvas_.drawFastVLine(cursorX, 68, 12, kAccent);
    canvas_.fillRect(0, 119, canvas_.width(), 16, kPanel);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(kMuted, kPanel);
    canvas_.drawString(clipped(footer, 38), canvas_.width() / 2, 127);
    canvas_.pushSprite(0, 0);
}

void UiRenderer::dialog(const String& title,
                        const String& line1,
                        const String& line2,
                        const String& footer,
                        uint16_t accentColor) {
    frame("CARDPUTER SSH", "", accentColor);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(accentColor, kBackground);
    canvas_.drawString(clipped(title, 36), canvas_.width() / 2, 39);
    canvas_.setTextColor(kForeground, kBackground);
    canvas_.drawString(clipped(line1, 38), canvas_.width() / 2, 64);
    canvas_.setTextColor(kMuted, kBackground);
    canvas_.drawString(clipped(line2, 38), canvas_.width() / 2, 83);
    canvas_.fillRect(0, 119, canvas_.width(), 16, kPanel);
    canvas_.setTextColor(kMuted, kPanel);
    canvas_.drawString(clipped(footer, 38), canvas_.width() / 2, 127);
    canvas_.pushSprite(0, 0);
}

void UiRenderer::fingerprint(const String& title,
                             const String& value,
                             const String& footer,
                             uint16_t accentColor) {
    frame("CARDPUTER SSH", "HOST KEY", accentColor);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(accentColor, kBackground);
    canvas_.drawString(clipped(title, 36), canvas_.width() / 2, 29);
    canvas_.setTextColor(kForeground, kBackground);
    canvas_.drawString(value.substring(0, min(static_cast<size_t>(38), value.length())), canvas_.width() / 2, 55);
    if (value.length() > 38) {
        canvas_.drawString(value.substring(38), canvas_.width() / 2, 72);
    }
    canvas_.fillRect(0, 119, canvas_.width(), 16, kPanel);
    canvas_.setTextColor(kMuted, kPanel);
    canvas_.drawString(clipped(footer, 38), canvas_.width() / 2, 127);
    canvas_.pushSprite(0, 0);
}

void UiRenderer::textBlock(const String& title,
                           const String& header,
                           const String& value,
                           const String& footer,
                           uint16_t accentColor) {
    constexpr size_t kColumns = 39;
    constexpr size_t kMaxLines = 8;
    frame("CARDPUTER SSH", title, accentColor);
    canvas_.setTextDatum(top_left);
    canvas_.setTextColor(accentColor, kBackground);
    canvas_.drawString(clipped(header, kColumns), 3, 20);
    canvas_.setTextColor(kForeground, kBackground);
    for (size_t line = 0; line < kMaxLines && line * kColumns < value.length(); ++line) {
        const size_t end = min(value.length(), (line + 1) * kColumns);
        canvas_.drawString(value.substring(line * kColumns, end), 3, 34 + static_cast<int>(line) * 10);
    }
    canvas_.fillRect(0, 119, canvas_.width(), 16, kPanel);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(kMuted, kPanel);
    canvas_.drawString(clipped(footer, 38), canvas_.width() / 2, 127);
    canvas_.pushSprite(0, 0);
}

void UiRenderer::invalidateTerminal() { terminalFrameValid_ = false; }

void UiRenderer::terminal(const UiStatus& status,
                          const TerminalEmulator& terminal,
                          const TerminalViewport& viewport,
                          bool localMode,
                          bool verified,
                          bool shortcutsVisible,
                          bool cursorBlinkOn) {
    const TerminalScreen& active = terminal.activeScreen();
    const TerminalCursor& cursor = active.cursor();
    const bool alternateSnapshot =
        localMode && viewport.alternateSnapshot();
    const bool showNew = terminalShowNew(
        localMode, alternateSnapshot,
        status.localHasNewOutput || viewport.hasNewOutput());
    const uint16_t modeColor = terminalModeColor(localMode, verified);
    const bool transition = !terminalFrameValid_ ||
                            lastTerminalLocal_ != localMode ||
                            lastShortcutVisible_ != shortcutsVisible;

    uint16_t rowsToDraw = transition ? kAllTerminalRows : 0;
    if (localMode) {
        const bool viewportMoved =
            lastViewportDistance_ != viewport.distanceFromBottom() ||
            lastViewportAlternate_ != alternateSnapshot;
        if (viewportMoved ||
            (!alternateSnapshot &&
             lastTerminalGeneration_ != terminal.generation())) {
            rowsToDraw = kAllTerminalRows;
        }
    } else {
        rowsToDraw = static_cast<uint16_t>(rowsToDraw | terminal.dirtyRows());
        if (lastCursorVisible_ || cursor.visible) {
            rowsToDraw = static_cast<uint16_t>(
                rowsToDraw | (1U << lastCursorRow_) | (1U << cursor.row));
        }
    }

    canvas_.setFont(&fonts::Font0);
    canvas_.setTextSize(1);
    canvas_.setTextWrap(false);
    canvas_.setTextDatum(top_left);

    // Status layout reserves mode and right-side pixels before assigning any
    // space to the host. The host is therefore always the first clipped item.
    canvas_.fillRect(0, 0, canvas_.width(), kTerminalStatusHeight, modeColor);
    const char* mode = terminalModeLabel(localMode);
    String right;
    if (showNew) right += "NEW ";
    if (status.modifier != nullptr && status.modifier[0] != '\0') {
        right += status.modifier;
        right += ' ';
    }
    right += String(status.rssi);
    right += ' ';
    right += String(status.batteryPercent);
    right += '%';

    const uint16_t modePixels = static_cast<uint16_t>(strlen(mode) *
                                                       kTerminalStatusCharacterWidth);
    const uint16_t rightPixels = static_cast<uint16_t>(
        right.length() * kTerminalStatusCharacterWidth);
    const TerminalStatusLayout layout = terminalStatusLayout(
        static_cast<uint16_t>(canvas_.width()), modePixels, rightPixels);
    const uint16_t statusText = localMode && verified ? kTerminalBackground
                                                     : kForeground;
    canvas_.setTextColor(statusText, modeColor);
    canvas_.setTextDatum(top_left);
    canvas_.drawString(mode, layout.modeX, 2);
    canvas_.setTextDatum(top_right);
    canvas_.drawString(right, canvas_.width() - kTerminalStatusInset, 2);

    const TerminalHostClip hostClip =
        terminalHostClip(status.host, layout.hostPixels);
    canvas_.setTextDatum(top_left);
    canvas_.setTextColor(statusText);
    if (terminalHostVisible(hostClip)) {
        if (hostClip.clipped && hostClip.characters == 0) {
            canvas_.setClipRect(layout.hostX, 2, layout.hostPixels,
                                kTerminalCellHeight);
            canvas_.drawChar('~', layout.hostX, 2);
        } else {
            for (uint8_t index = 0; index < hostClip.characters; ++index) {
                const int x = layout.hostX +
                              index * kTerminalStatusCharacterWidth;
                canvas_.setClipRect(x, 2, kTerminalStatusCharacterWidth,
                                    kTerminalCellHeight);
                canvas_.drawChar(static_cast<uint8_t>(status.host[index]), x,
                                 2);
            }
            if (hostClip.clipped) {
                const int x =
                    layout.hostX +
                    hostClip.characters * kTerminalStatusCharacterWidth;
                canvas_.setClipRect(x, 2, kTerminalStatusCharacterWidth,
                                    kTerminalCellHeight);
                canvas_.drawChar('~', x, 2);
            }
        }
        canvas_.clearClipRect();
    }

    for (size_t row = 0; row < kTerminalRows; ++row) {
        if ((rowsToDraw & (1U << row)) == 0) continue;
        for (size_t column = 0; column < kTerminalColumns; ++column) {
            const TerminalCell& cell =
                localMode ? viewport.cell(terminal, row, column)
                          : active.cell(row, column);
            const bool cursorCell =
                !localMode && cursor.visible && cursorBlinkOn &&
                cursor.row == row && cursor.column == column;
            drawTerminalCell(canvas_, cell, terminalCellX(column),
                             terminalCellY(row), cursorCell);
        }
    }

    if (localMode && !alternateSnapshot) {
        const int thumbHeight = kTerminalCellHeight;
        constexpr int travel =
            kTerminalRows * kTerminalCellHeight - thumbHeight;
        const size_t historySize = terminal.history().size();
        const size_t distance = viewport.distanceFromBottom() > historySize
                                    ? historySize
                                    : viewport.distanceFromBottom();
        const int offset = historySize == 0
                               ? travel
                               : travel - static_cast<int>(
                                              distance * travel / historySize);
        canvas_.fillRect(kTerminalScrollIndicatorX,
                         kTerminalStatusHeight + offset,
                         kTerminalScrollIndicatorWidth, thumbHeight,
                         kTerminalLocalColor);
    }

    canvas_.fillRect(0, kTerminalRailY, canvas_.width(),
                     kTerminalRailHeight, modeColor);
    if (shortcutsVisible) {
        if (localMode) {
            drawCompactLine(canvas_, terminalLocalShortcutFirstText(),
                            terminalCellY(terminalLocalShortcutFirstRow()),
                            kForeground, kPanel);
            drawCompactLine(canvas_, terminalLocalShortcutSecondText(),
                            terminalCellY(terminalLocalShortcutSecondRow()),
                            kForeground, kPanel);
        } else {
            drawCompactLine(canvas_, terminalRemoteShortcutText(),
                            terminalCellY(terminalRemoteShortcutRow()),
                            kForeground, kPanel);
        }
    }

    canvas_.clearClipRect();
    canvas_.pushSprite(0, 0);

    terminalFrameValid_ = true;
    lastTerminalGeneration_ = terminal.generation();
    lastViewportDistance_ = viewport.distanceFromBottom();
    lastCursorRow_ = cursor.row;
    lastTerminalLocal_ = localMode;
    lastShortcutVisible_ = shortcutsVisible;
    lastCursorVisible_ = cursor.visible;
    lastViewportAlternate_ = alternateSnapshot;
}

void UiRenderer::frame(const String& title, const String& right, uint16_t color) {
    terminalFrameValid_ = false;
    canvas_.fillSprite(kBackground);
    canvas_.fillRect(0, 0, canvas_.width(), 14, color);
    canvas_.setTextDatum(top_left);
    canvas_.setTextColor(kBackground, color);
    canvas_.drawString(clipped(title, 22), 3, 3);
    canvas_.setTextDatum(top_right);
    canvas_.drawString(clipped(right, 14), canvas_.width() - 3, 3);
    canvas_.setTextDatum(top_left);
}

String UiRenderer::clipped(const String& value, size_t maximum) {
    if (maximum == 0) {
        return "";
    }
    if (value.length() <= maximum) {
        return value;
    }
    return value.substring(0, maximum - 1) + "~";
}

}  // namespace app
