#include "TerminalGlyphs.h"

#include <stddef.h>

namespace app {
namespace {

struct GlyphMaskEntry {
    uint16_t glyph;
    uint8_t columns[5];
};

constexpr GlyphMaskEntry kArrowMasks[] = {
    {0x2190, {0x38, 0x54, 0x10, 0x10, 0x10}},  // left
    {0x2191, {0x04, 0x02, 0xfd, 0x02, 0x04}},  // up
    {0x2192, {0x10, 0x10, 0x10, 0x54, 0x38}},  // right
    {0x2193, {0x20, 0x40, 0xbf, 0x40, 0x20}},  // down
    {0x2194, {0x38, 0x54, 0x10, 0x54, 0x38}},  // left-right
    {0x2195, {0x24, 0x42, 0xbd, 0x42, 0x24}},  // up-down
    {0x2196, {0x07, 0x03, 0x05, 0x08, 0x10}},  // north-west
    {0x2197, {0x10, 0x08, 0x05, 0x03, 0x07}},  // north-east
    {0x2198, {0x08, 0x10, 0xa0, 0xc0, 0xe0}},  // south-east
    {0x2199, {0xe0, 0xc0, 0xa0, 0x10, 0x08}},  // south-west
    {0x21a9, {0x38, 0x54, 0x10, 0x90, 0xf0}},  // left with hook
    {0x21aa, {0xf0, 0x90, 0x10, 0x54, 0x38}},  // right with hook
    {0x21ba, {0x1e, 0x21, 0x69, 0x29, 0x18}},  // anticlockwise circle
    {0x21bb, {0x18, 0x29, 0x69, 0x21, 0x1e}},  // clockwise circle
    {0x21d0, {0x5c, 0x7e, 0x6a, 0x48, 0x48}},  // double left
    {0x21d1, {0x0a, 0x7d, 0x0a, 0x7d, 0x0a}},  // double up
    {0x21d2, {0x48, 0x48, 0x6a, 0x7e, 0x5c}},  // double right
    {0x21d3, {0x50, 0xbe, 0x50, 0xbe, 0x50}},  // double down
    {0x21d4, {0x36, 0x55, 0x14, 0x55, 0x36}},  // double left-right
    {0x21d5, {0xaa, 0xdd, 0xaa, 0xdd, 0xaa}},  // double up-down
    {0x21d6, {0x0f, 0x07, 0x0f, 0x18, 0x30}},  // double north-west
    {0x21d7, {0x30, 0x18, 0x0f, 0x07, 0x0f}},  // double north-east
    {0x21d8, {0x0c, 0x18, 0xf0, 0xe0, 0xf0}},  // double south-east
    {0x21d9, {0xf0, 0xe0, 0xf0, 0x18, 0x0c}},  // double south-west
    {0x21e6, {0x1c, 0x22, 0x14, 0x08, 0x08}},  // white left
    {0x21e7, {0x04, 0x0a, 0x71, 0x0a, 0x04}},  // white up
    {0x21e8, {0x08, 0x08, 0x14, 0x22, 0x1c}},  // white right
    {0x21e9, {0x20, 0x50, 0x8e, 0x50, 0x20}},  // white down
    {0x21fd, {0x28, 0x54, 0x92, 0x10, 0x10}},  // open-headed left
    {0x21fe, {0x10, 0x10, 0x92, 0x54, 0x28}},  // open-headed right
    {0x21ff, {0x28, 0x54, 0x92, 0x54, 0x28}},  // open-headed left-right
};

constexpr GlyphMaskEntry kGeometryMasks[] = {
    {0x25a0, {0x7c, 0x7c, 0x7c, 0x7c, 0x7c}},  // black square
    {0x25a1, {0x7c, 0x44, 0x44, 0x44, 0x7c}},  // white square
    {0x25b2, {0x30, 0x3c, 0x3e, 0x3c, 0x30}},  // black up triangle
    {0x25b6, {0x7f, 0x3e, 0x1c, 0x08, 0x00}},  // black right triangle
    {0x25bc, {0x0c, 0x3c, 0x7c, 0x3c, 0x0c}},  // black down triangle
    {0x25c0, {0x00, 0x08, 0x1c, 0x3e, 0x7f}},  // black left triangle
    {0x25c6, {0x08, 0x1c, 0x3e, 0x1c, 0x08}},  // black diamond
    {0x25cb, {0x1c, 0x22, 0x41, 0x22, 0x1c}},  // white circle
    {0x25cf, {0x1c, 0x3e, 0x7f, 0x3e, 0x1c}},  // black circle
};

// Dedicated Unicode U+00A0-U+00BF masks. These are indexed by Unicode code
// point and do not rely on the unrelated CP437 byte positions used by Font0.
constexpr uint8_t kLatin1SymbolMasks[32][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},  // U+00A0 no-break space
    {0x00, 0x00, 0xbe, 0x00, 0x00},  // U+00A1 inverted exclamation
    {0x38, 0x44, 0xfe, 0x44, 0x28},  // U+00A2 cent
    {0x48, 0x7e, 0x49, 0x49, 0x4a},  // U+00A3 pound
    {0x22, 0x1c, 0x14, 0x1c, 0x22},  // U+00A4 currency
    {0x05, 0x0a, 0xf0, 0x0a, 0x05},  // U+00A5 yen
    {0x00, 0x00, 0x77, 0x00, 0x00},  // U+00A6 broken bar
    {0x4c, 0x92, 0x92, 0x92, 0x64},  // U+00A7 section
    {0x00, 0x01, 0x00, 0x01, 0x00},  // U+00A8 diaeresis
    {0x3e, 0x41, 0x5d, 0x55, 0x22},  // U+00A9 copyright
    {0x24, 0x2a, 0x2a, 0x2a, 0x3c},  // U+00AA feminine ordinal
    {0x08, 0x14, 0x2a, 0x14, 0x22},  // U+00AB left guillemet
    {0x08, 0x08, 0x38, 0x20, 0x20},  // U+00AC not sign
    {0x10, 0x10, 0x10, 0x10, 0x10},  // U+00AD soft hyphen
    {0x3e, 0x41, 0x5d, 0x4d, 0x32},  // U+00AE registered
    {0x01, 0x01, 0x01, 0x01, 0x01},  // U+00AF macron
    {0x00, 0x06, 0x09, 0x09, 0x06},  // U+00B0 degree
    {0x44, 0x54, 0x7c, 0x54, 0x44},  // U+00B1 plus-minus
    {0x00, 0x09, 0x0d, 0x0b, 0x09},  // U+00B2 superscript two
    {0x00, 0x09, 0x0b, 0x0d, 0x09},  // U+00B3 superscript three
    {0x00, 0x02, 0x01, 0x00, 0x00},  // U+00B4 acute
    {0xfc, 0x20, 0x20, 0x20, 0x1c},  // U+00B5 micro
    {0x06, 0x09, 0xff, 0x01, 0xff},  // U+00B6 pilcrow
    {0x00, 0x00, 0x10, 0x00, 0x00},  // U+00B7 middle dot
    {0x00, 0x00, 0x80, 0x40, 0x00},  // U+00B8 cedilla
    {0x00, 0x0a, 0x0f, 0x08, 0x00},  // U+00B9 superscript one
    {0x24, 0x2a, 0x2a, 0x2a, 0x1c},  // U+00BA masculine ordinal
    {0x22, 0x14, 0x2a, 0x14, 0x08},  // U+00BB right guillemet
    {0x62, 0x14, 0x08, 0x74, 0x22},  // U+00BC one quarter
    {0x62, 0x14, 0x08, 0x52, 0x62},  // U+00BD one half
    {0x52, 0x34, 0x08, 0x74, 0x22},  // U+00BE three quarters
    {0x00, 0x80, 0xba, 0x90, 0x00},  // U+00BF inverted question
};

enum class Diacritic : uint8_t { Grave, Acute, Circumflex, Tilde, Diaeresis, Ring };

bool inRange(uint32_t value, uint32_t first, uint32_t last) {
    return value >= first && value <= last;
}

void setMask(uint8_t columns[5], uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3,
             uint8_t c4) {
    columns[0] = c0;
    columns[1] = c1;
    columns[2] = c2;
    columns[3] = c3;
    columns[4] = c4;
}

void copyMask(uint8_t columns[5], const uint8_t source[5]) {
    for (size_t index = 0; index < 5; ++index) columns[index] = source[index];
}

bool findMask(const GlyphMaskEntry* entries, size_t count, uint16_t glyph,
              uint8_t columns[5]) {
    for (size_t index = 0; index < count; ++index) {
        if (entries[index].glyph == glyph) {
            if (columns != nullptr) copyMask(columns, entries[index].columns);
            return true;
        }
    }
    return false;
}

bool supportedArrow(uint16_t glyph) {
    return findMask(kArrowMasks, sizeof(kArrowMasks) / sizeof(kArrowMasks[0]), glyph, nullptr);
}

bool supportedGeometry(uint16_t glyph) {
    return findMask(kGeometryMasks, sizeof(kGeometryMasks) / sizeof(kGeometryMasks[0]), glyph,
                    nullptr);
}

bool baseLatinLetter(char letter, uint8_t columns[5]) {
    switch (letter) {
        case 'A': setMask(columns, 0xf8, 0x14, 0x14, 0x14, 0xf8); return true;
        case 'C': setMask(columns, 0x78, 0x84, 0x84, 0x84, 0x48); return true;
        case 'E': setMask(columns, 0xfc, 0x94, 0x94, 0x94, 0x84); return true;
        case 'I': setMask(columns, 0x84, 0x84, 0xfc, 0x84, 0x84); return true;
        case 'N': setMask(columns, 0xfc, 0x08, 0x10, 0x20, 0xfc); return true;
        case 'O': setMask(columns, 0x78, 0x84, 0x84, 0x84, 0x78); return true;
        case 'U': setMask(columns, 0x7c, 0x80, 0x80, 0x80, 0x7c); return true;
        case 'Y': setMask(columns, 0x04, 0x08, 0xf0, 0x08, 0x04); return true;
        case 'a': setMask(columns, 0x60, 0x94, 0x94, 0x94, 0xf8); return true;
        case 'c': setMask(columns, 0x70, 0x88, 0x88, 0x88, 0x40); return true;
        case 'e': setMask(columns, 0x38, 0x54, 0x54, 0x54, 0x18); return true;
        case 'i': setMask(columns, 0x00, 0x80, 0xf8, 0x80, 0x00); return true;
        case 'n': setMask(columns, 0x7c, 0x04, 0x04, 0x04, 0x78); return true;
        case 'o': setMask(columns, 0x38, 0x44, 0x44, 0x44, 0x38); return true;
        case 'u': setMask(columns, 0x3c, 0x40, 0x40, 0x40, 0x7c); return true;
        case 'y': setMask(columns, 0x4c, 0x90, 0x90, 0x90, 0x7c); return true;
        default: return false;
    }
}

void applyDiacritic(Diacritic diacritic, uint8_t columns[5]) {
    switch (diacritic) {
        case Diacritic::Grave:
            columns[1] |= 0x01;
            columns[2] |= 0x02;
            break;
        case Diacritic::Acute:
            columns[2] |= 0x02;
            columns[3] |= 0x01;
            break;
        case Diacritic::Circumflex:
            columns[1] |= 0x02;
            columns[2] |= 0x01;
            columns[3] |= 0x02;
            break;
        case Diacritic::Tilde:
            columns[0] |= 0x02;
            columns[1] |= 0x01;
            columns[2] |= 0x02;
            columns[3] |= 0x01;
            columns[4] |= 0x02;
            break;
        case Diacritic::Diaeresis:
            columns[1] |= 0x01;
            columns[3] |= 0x01;
            break;
        case Diacritic::Ring:
            columns[1] |= 0x02;
            columns[2] |= 0x01;
            columns[3] |= 0x02;
            break;
    }
}

bool accentedLatinLetter(char letter, Diacritic diacritic, uint8_t columns[5]) {
    if (!baseLatinLetter(letter, columns)) return false;
    applyDiacritic(diacritic, columns);
    return true;
}

bool cedillaLatinLetter(char letter, uint8_t columns[5]) {
    if (!baseLatinLetter(letter, columns)) return false;
    for (size_t index = 0; index < 5; ++index) columns[index] >>= 1;
    columns[2] |= 0x80;
    return true;
}

bool latin1LetterMask(uint16_t glyph, uint8_t columns[5]) {
    switch (glyph) {
        case 0x00c0: return accentedLatinLetter('A', Diacritic::Grave, columns);
        case 0x00c1: return accentedLatinLetter('A', Diacritic::Acute, columns);
        case 0x00c2: return accentedLatinLetter('A', Diacritic::Circumflex, columns);
        case 0x00c3: return accentedLatinLetter('A', Diacritic::Tilde, columns);
        case 0x00c4: return accentedLatinLetter('A', Diacritic::Diaeresis, columns);
        case 0x00c5: return accentedLatinLetter('A', Diacritic::Ring, columns);
        case 0x00c6: setMask(columns, 0xfc, 0x14, 0xfc, 0x94, 0x84); return true;
        case 0x00c7: return cedillaLatinLetter('C', columns);
        case 0x00c8: return accentedLatinLetter('E', Diacritic::Grave, columns);
        case 0x00c9: return accentedLatinLetter('E', Diacritic::Acute, columns);
        case 0x00ca: return accentedLatinLetter('E', Diacritic::Circumflex, columns);
        case 0x00cb: return accentedLatinLetter('E', Diacritic::Diaeresis, columns);
        case 0x00cc: return accentedLatinLetter('I', Diacritic::Grave, columns);
        case 0x00cd: return accentedLatinLetter('I', Diacritic::Acute, columns);
        case 0x00ce: return accentedLatinLetter('I', Diacritic::Circumflex, columns);
        case 0x00cf: return accentedLatinLetter('I', Diacritic::Diaeresis, columns);
        case 0x00d0: setMask(columns, 0xfc, 0x94, 0x84, 0x84, 0x78); return true;
        case 0x00d1: return accentedLatinLetter('N', Diacritic::Tilde, columns);
        case 0x00d2: return accentedLatinLetter('O', Diacritic::Grave, columns);
        case 0x00d3: return accentedLatinLetter('O', Diacritic::Acute, columns);
        case 0x00d4: return accentedLatinLetter('O', Diacritic::Circumflex, columns);
        case 0x00d5: return accentedLatinLetter('O', Diacritic::Tilde, columns);
        case 0x00d6: return accentedLatinLetter('O', Diacritic::Diaeresis, columns);
        case 0x00d7: setMask(columns, 0x22, 0x14, 0x08, 0x14, 0x22); return true;
        case 0x00d8: setMask(columns, 0xf8, 0xc4, 0xa4, 0x94, 0x78); return true;
        case 0x00d9: return accentedLatinLetter('U', Diacritic::Grave, columns);
        case 0x00da: return accentedLatinLetter('U', Diacritic::Acute, columns);
        case 0x00db: return accentedLatinLetter('U', Diacritic::Circumflex, columns);
        case 0x00dc: return accentedLatinLetter('U', Diacritic::Diaeresis, columns);
        case 0x00dd: return accentedLatinLetter('Y', Diacritic::Acute, columns);
        case 0x00de: setMask(columns, 0xff, 0x12, 0x12, 0x12, 0x0c); return true;
        case 0x00df: setMask(columns, 0xfc, 0x08, 0x14, 0x14, 0x68); return true;
        case 0x00e0: return accentedLatinLetter('a', Diacritic::Grave, columns);
        case 0x00e1: return accentedLatinLetter('a', Diacritic::Acute, columns);
        case 0x00e2: return accentedLatinLetter('a', Diacritic::Circumflex, columns);
        case 0x00e3: return accentedLatinLetter('a', Diacritic::Tilde, columns);
        case 0x00e4: return accentedLatinLetter('a', Diacritic::Diaeresis, columns);
        case 0x00e5: return accentedLatinLetter('a', Diacritic::Ring, columns);
        case 0x00e6: setMask(columns, 0x38, 0x54, 0x7c, 0x54, 0x18); return true;
        case 0x00e7: return cedillaLatinLetter('c', columns);
        case 0x00e8: return accentedLatinLetter('e', Diacritic::Grave, columns);
        case 0x00e9: return accentedLatinLetter('e', Diacritic::Acute, columns);
        case 0x00ea: return accentedLatinLetter('e', Diacritic::Circumflex, columns);
        case 0x00eb: return accentedLatinLetter('e', Diacritic::Diaeresis, columns);
        case 0x00ec: return accentedLatinLetter('i', Diacritic::Grave, columns);
        case 0x00ed: return accentedLatinLetter('i', Diacritic::Acute, columns);
        case 0x00ee: return accentedLatinLetter('i', Diacritic::Circumflex, columns);
        case 0x00ef: return accentedLatinLetter('i', Diacritic::Diaeresis, columns);
        case 0x00f0: setMask(columns, 0x38, 0x54, 0x7c, 0x44, 0x38); return true;
        case 0x00f1: return accentedLatinLetter('n', Diacritic::Tilde, columns);
        case 0x00f2: return accentedLatinLetter('o', Diacritic::Grave, columns);
        case 0x00f3: return accentedLatinLetter('o', Diacritic::Acute, columns);
        case 0x00f4: return accentedLatinLetter('o', Diacritic::Circumflex, columns);
        case 0x00f5: return accentedLatinLetter('o', Diacritic::Tilde, columns);
        case 0x00f6: return accentedLatinLetter('o', Diacritic::Diaeresis, columns);
        case 0x00f7: setMask(columns, 0x10, 0x54, 0x10, 0x54, 0x10); return true;
        case 0x00f8: setMask(columns, 0xb8, 0x64, 0x54, 0x4c, 0x38); return true;
        case 0x00f9: return accentedLatinLetter('u', Diacritic::Grave, columns);
        case 0x00fa: return accentedLatinLetter('u', Diacritic::Acute, columns);
        case 0x00fb: return accentedLatinLetter('u', Diacritic::Circumflex, columns);
        case 0x00fc: return accentedLatinLetter('u', Diacritic::Diaeresis, columns);
        case 0x00fd: return accentedLatinLetter('y', Diacritic::Acute, columns);
        case 0x00fe: setMask(columns, 0xff, 0x48, 0x48, 0x48, 0x30); return true;
        case 0x00ff: return accentedLatinLetter('y', Diacritic::Diaeresis, columns);
        default: return false;
    }
}

bool latin1Mask(uint16_t glyph, uint8_t columns[5]) {
    if (inRange(glyph, 0x00a0, 0x00bf)) {
        copyMask(columns, kLatin1SymbolMasks[glyph - 0x00a0]);
        return true;
    }
    return latin1LetterMask(glyph, columns);
}

}  // namespace

uint16_t normalizeTerminalCodePoint(uint32_t codePoint) {
    const uint16_t narrow = static_cast<uint16_t>(codePoint);
    if (inRange(codePoint, 0x20, 0x7e) || inRange(codePoint, 0x00a0, 0x00ff) ||
        codePoint == 0x2022 || codePoint == 0x2026 ||
        (codePoint <= 0xffff && supportedArrow(narrow)) ||
        inRange(codePoint, 0x2500, 0x257f) || inRange(codePoint, 0x2580, 0x259f) ||
        (codePoint <= 0xffff && supportedGeometry(narrow)) ||
        inRange(codePoint, 0x2800, 0x28ff) || codePoint == kTerminalReplacementGlyph) {
        return narrow;
    }
    return kTerminalReplacementGlyph;
}

TerminalGlyphKind terminalGlyphKind(uint16_t glyph) {
    if (inRange(glyph, 0x20, 0x7e)) return TerminalGlyphKind::Builtin;
    if (inRange(glyph, 0x00a0, 0x00ff)) return TerminalGlyphKind::Latin1;
    if (supportedArrow(glyph)) return TerminalGlyphKind::Arrow;
    if (inRange(glyph, 0x2500, 0x257f)) return TerminalGlyphKind::BoxDrawing;
    if (inRange(glyph, 0x2580, 0x259f)) return TerminalGlyphKind::Block;
    if (supportedGeometry(glyph) || glyph == 0x2022 || glyph == 0x2026) {
        return TerminalGlyphKind::Geometric;
    }
    if (inRange(glyph, 0x2800, 0x28ff)) return TerminalGlyphKind::Braille;
    return TerminalGlyphKind::Replacement;
}

bool terminalGlyphMask(uint16_t glyph, uint8_t columns[5]) {
    if (columns == nullptr) return false;
    setMask(columns, 0, 0, 0, 0, 0);

    if (inRange(glyph, 0x00a0, 0x00ff)) return latin1Mask(glyph, columns);
    if (findMask(kArrowMasks, sizeof(kArrowMasks) / sizeof(kArrowMasks[0]), glyph,
                 columns)) {
        return true;
    }
    if (findMask(kGeometryMasks, sizeof(kGeometryMasks) / sizeof(kGeometryMasks[0]), glyph,
                 columns)) {
        return true;
    }
    if (glyph == 0x2022) {
        setMask(columns, 0x00, 0x0c, 0x1e, 0x0c, 0x00);
        return true;
    }
    if (glyph == 0x2026) {
        setMask(columns, 0x40, 0x00, 0x40, 0x00, 0x40);
        return true;
    }
    if (glyph == kTerminalReplacementGlyph) {
        setMask(columns, 0x1c, 0x22, 0x49, 0x22, 0x1c);
        return true;
    }
    return false;
}

}  // namespace app
