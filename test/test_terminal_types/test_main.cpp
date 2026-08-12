#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "TerminalGeometry.h"
#include "TerminalGlyphs.h"
#include "TerminalPalette.h"
#include "TerminalTypes.h"

void setUp() {}
void tearDown() {}

namespace {

void assertGlyphMask(uint16_t glyph, const uint8_t (&expected)[5]) {
    uint8_t actual[5]{};
    TEST_ASSERT_TRUE(app::terminalGlyphMask(glyph, actual));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, 5);
}

void assertGlyphMasksDiffer(uint16_t uppercase, uint16_t lowercase) {
    uint8_t upperMask[5]{};
    uint8_t lowerMask[5]{};
    TEST_ASSERT_TRUE(app::terminalGlyphMask(uppercase, upperMask));
    TEST_ASSERT_TRUE(app::terminalGlyphMask(lowercase, lowerMask));
    bool differs = false;
    for (size_t column = 0; column < 5; ++column) {
        differs = differs || upperMask[column] != lowerMask[column];
    }
    TEST_ASSERT_TRUE(differs);
}

void testFixedGeometryAndCellStorage() {
    TEST_ASSERT_EQUAL_UINT16(40, app::kTerminalColumns);
    TEST_ASSERT_EQUAL_UINT16(13, app::kTerminalRows);
    TEST_ASSERT_EQUAL_UINT16(120, app::kTerminalHistoryRows);
    TEST_ASSERT_EQUAL_UINT8(6, app::kTerminalCellWidth);
    TEST_ASSERT_EQUAL_UINT8(9, app::kTerminalCellHeight);
    TEST_ASSERT_EQUAL_UINT8(15, app::kTerminalStatusHeight);
    TEST_ASSERT_EQUAL_UINT8(3, app::kTerminalRailHeight);
    TEST_ASSERT_EQUAL_UINT(5, sizeof(app::TerminalCell));

    app::TerminalCell cell;
    TEST_ASSERT_EQUAL_HEX16(' ', cell.glyph());
    TEST_ASSERT_EQUAL_UINT8(7, cell.foreground());
    TEST_ASSERT_EQUAL_UINT8(0, cell.background());
    TEST_ASSERT_EQUAL_UINT8(0, cell.attributes());

    cell.setGlyph(0x2500);
    cell.setForeground(196);
    cell.setBackground(17);
    cell.setAttributes(app::kTerminalBold | app::kTerminalUnderline);
    TEST_ASSERT_EQUAL_HEX16(0x2500, cell.glyph());
    TEST_ASSERT_EQUAL_UINT8(196, cell.foreground());
    TEST_ASSERT_EQUAL_UINT8(17, cell.background());
    TEST_ASSERT_EQUAL_UINT8(app::kTerminalBold | app::kTerminalUnderline, cell.attributes());
}

void testSupportedGlyphRangesAndReplacement() {
    TEST_ASSERT_EQUAL_HEX16('A', app::normalizeTerminalCodePoint('A'));
    TEST_ASSERT_EQUAL_HEX16(0x00e9, app::normalizeTerminalCodePoint(0x00e9));
    TEST_ASSERT_EQUAL_HEX16(0x2022, app::normalizeTerminalCodePoint(0x2022));
    TEST_ASSERT_EQUAL_HEX16(0x2026, app::normalizeTerminalCodePoint(0x2026));
    TEST_ASSERT_EQUAL_HEX16(0x2192, app::normalizeTerminalCodePoint(0x2192));
    TEST_ASSERT_EQUAL_HEX16(0x21a9, app::normalizeTerminalCodePoint(0x21a9));
    TEST_ASSERT_EQUAL_HEX16(0x21bb, app::normalizeTerminalCodePoint(0x21bb));
    TEST_ASSERT_EQUAL_HEX16(0x21d7, app::normalizeTerminalCodePoint(0x21d7));
    TEST_ASSERT_EQUAL_HEX16(0x21e7, app::normalizeTerminalCodePoint(0x21e7));
    TEST_ASSERT_EQUAL_HEX16(0x21ff, app::normalizeTerminalCodePoint(0x21ff));
    TEST_ASSERT_EQUAL_HEX16(0xfffd, app::normalizeTerminalCodePoint(0x219a));
    TEST_ASSERT_EQUAL_HEX16(0xfffd, app::normalizeTerminalCodePoint(0x21ab));
    TEST_ASSERT_EQUAL_HEX16(0xfffd, app::normalizeTerminalCodePoint(0x21dc));
    TEST_ASSERT_EQUAL_HEX16(0x2500, app::normalizeTerminalCodePoint(0x2500));
    TEST_ASSERT_EQUAL_HEX16(0x2588, app::normalizeTerminalCodePoint(0x2588));
    TEST_ASSERT_EQUAL_HEX16(0x25cf, app::normalizeTerminalCodePoint(0x25cf));
    TEST_ASSERT_EQUAL_HEX16(0xfffd, app::normalizeTerminalCodePoint(0x25a2));
    TEST_ASSERT_EQUAL_HEX16(0xfffd, app::normalizeTerminalCodePoint(0x25ff));
    TEST_ASSERT_EQUAL_HEX16(0x2801, app::normalizeTerminalCodePoint(0x2801));
    TEST_ASSERT_EQUAL_HEX16(0xfffd, app::normalizeTerminalCodePoint(0x4e2d));
    TEST_ASSERT_EQUAL_HEX16(0xfffd, app::normalizeTerminalCodePoint(0x110000));
}

void testLatin1UsesDedicatedUnicodeMasks() {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::TerminalGlyphKind::Latin1),
                            static_cast<uint8_t>(app::terminalGlyphKind(0x00e9)));
    const uint8_t pound[5] = {0x48, 0x7e, 0x49, 0x49, 0x4a};
    const uint8_t eAcute[5] = {0x38, 0x54, 0x56, 0x55, 0x18};
    const uint8_t nTilde[5] = {0x7e, 0x05, 0x06, 0x05, 0x7a};
    assertGlyphMask(0x00a3, pound);
    assertGlyphMask(0x00e9, eAcute);
    assertGlyphMask(0x00f1, nTilde);

    for (uint16_t glyph = 0x00a0; glyph <= 0x00ff; ++glyph) {
        uint8_t mask[5]{};
        TEST_ASSERT_TRUE(app::terminalGlyphMask(glyph, mask));
    }
}

void testLatin1CasePairsRemainDistinct() {
    const uint8_t upperCedilla[5] = {0x3c, 0x42, 0xc2, 0x42, 0x24};
    const uint8_t lowerCedilla[5] = {0x38, 0x44, 0xc4, 0x44, 0x20};
    const uint8_t upperThorn[5] = {0xff, 0x12, 0x12, 0x12, 0x0c};
    const uint8_t lowerThorn[5] = {0xff, 0x48, 0x48, 0x48, 0x30};
    assertGlyphMask(0x00c7, upperCedilla);
    assertGlyphMask(0x00e7, lowerCedilla);
    assertGlyphMask(0x00de, upperThorn);
    assertGlyphMask(0x00fe, lowerThorn);

    const uint16_t casePairs[][2] = {
        {0x00c1, 0x00e1}, {0x00c7, 0x00e7}, {0x00c9, 0x00e9},
        {0x00cd, 0x00ed}, {0x00d1, 0x00f1}, {0x00d6, 0x00f6},
        {0x00dc, 0x00fc}, {0x00dd, 0x00fd}, {0x00c6, 0x00e6},
        {0x00d0, 0x00f0}, {0x00d8, 0x00f8}, {0x00de, 0x00fe},
    };
    for (const auto& pair : casePairs) assertGlyphMasksDiffer(pair[0], pair[1]);
}

void testAccentedLowercaseIUsesDotlessBase() {
    const uint8_t iGrave[5] = {0x00, 0x81, 0xfa, 0x80, 0x00};
    const uint8_t iAcute[5] = {0x00, 0x80, 0xfa, 0x81, 0x00};
    const uint8_t iCircumflex[5] = {0x00, 0x82, 0xf9, 0x82, 0x00};
    const uint8_t iDiaeresis[5] = {0x00, 0x81, 0xf8, 0x81, 0x00};
    assertGlyphMask(0x00ec, iGrave);
    assertGlyphMask(0x00ed, iAcute);
    assertGlyphMask(0x00ee, iCircumflex);
    assertGlyphMask(0x00ef, iDiaeresis);
}

void testEveryAcceptedArrowHasItsExactMask() {
    struct ArrowExpectation {
        uint16_t glyph;
        uint8_t mask[5];
    };
    static const ArrowExpectation arrows[] = {
        {0x2190, {0x38, 0x54, 0x10, 0x10, 0x10}},
        {0x2191, {0x04, 0x02, 0xfd, 0x02, 0x04}},
        {0x2192, {0x10, 0x10, 0x10, 0x54, 0x38}},
        {0x2193, {0x20, 0x40, 0xbf, 0x40, 0x20}},
        {0x2194, {0x38, 0x54, 0x10, 0x54, 0x38}},
        {0x2195, {0x24, 0x42, 0xbd, 0x42, 0x24}},
        {0x2196, {0x07, 0x03, 0x05, 0x08, 0x10}},
        {0x2197, {0x10, 0x08, 0x05, 0x03, 0x07}},
        {0x2198, {0x08, 0x10, 0xa0, 0xc0, 0xe0}},
        {0x2199, {0xe0, 0xc0, 0xa0, 0x10, 0x08}},
        {0x21a9, {0x38, 0x54, 0x10, 0x90, 0xf0}},
        {0x21aa, {0xf0, 0x90, 0x10, 0x54, 0x38}},
        {0x21ba, {0x1e, 0x21, 0x69, 0x29, 0x18}},
        {0x21bb, {0x18, 0x29, 0x69, 0x21, 0x1e}},
        {0x21d0, {0x5c, 0x7e, 0x6a, 0x48, 0x48}},
        {0x21d1, {0x0a, 0x7d, 0x0a, 0x7d, 0x0a}},
        {0x21d2, {0x48, 0x48, 0x6a, 0x7e, 0x5c}},
        {0x21d3, {0x50, 0xbe, 0x50, 0xbe, 0x50}},
        {0x21d4, {0x36, 0x55, 0x14, 0x55, 0x36}},
        {0x21d5, {0xaa, 0xdd, 0xaa, 0xdd, 0xaa}},
        {0x21d6, {0x0f, 0x07, 0x0f, 0x18, 0x30}},
        {0x21d7, {0x30, 0x18, 0x0f, 0x07, 0x0f}},
        {0x21d8, {0x0c, 0x18, 0xf0, 0xe0, 0xf0}},
        {0x21d9, {0xf0, 0xe0, 0xf0, 0x18, 0x0c}},
        {0x21e6, {0x1c, 0x22, 0x14, 0x08, 0x08}},
        {0x21e7, {0x04, 0x0a, 0x71, 0x0a, 0x04}},
        {0x21e8, {0x08, 0x08, 0x14, 0x22, 0x1c}},
        {0x21e9, {0x20, 0x50, 0x8e, 0x50, 0x20}},
        {0x21fd, {0x28, 0x54, 0x92, 0x10, 0x10}},
        {0x21fe, {0x10, 0x10, 0x92, 0x54, 0x28}},
        {0x21ff, {0x28, 0x54, 0x92, 0x54, 0x28}},
    };
    for (const ArrowExpectation& arrow : arrows) {
        assertGlyphMask(arrow.glyph, arrow.mask);
    }

    uint8_t mask[5]{};
    TEST_ASSERT_FALSE(app::terminalGlyphMask(0x219a, mask));
    TEST_ASSERT_FALSE(app::terminalGlyphMask(0x21ab, mask));
    TEST_ASSERT_FALSE(app::terminalGlyphMask(0x21dc, mask));
}

void testOnlyImplementedGeometriesAreAccepted() {
    struct GeometryExpectation {
        uint16_t glyph;
        uint8_t mask[5];
    };
    static const GeometryExpectation geometries[] = {
        {0x25a0, {0x7c, 0x7c, 0x7c, 0x7c, 0x7c}},
        {0x25a1, {0x7c, 0x44, 0x44, 0x44, 0x7c}},
        {0x25b2, {0x30, 0x3c, 0x3e, 0x3c, 0x30}},
        {0x25b6, {0x7f, 0x3e, 0x1c, 0x08, 0x00}},
        {0x25bc, {0x0c, 0x3c, 0x7c, 0x3c, 0x0c}},
        {0x25c0, {0x00, 0x08, 0x1c, 0x3e, 0x7f}},
        {0x25c6, {0x08, 0x1c, 0x3e, 0x1c, 0x08}},
        {0x25cb, {0x1c, 0x22, 0x41, 0x22, 0x1c}},
        {0x25cf, {0x1c, 0x3e, 0x7f, 0x3e, 0x1c}},
    };
    for (const GeometryExpectation& geometry : geometries) {
        assertGlyphMask(geometry.glyph, geometry.mask);
    }

    uint8_t mask[5]{};
    TEST_ASSERT_FALSE(app::terminalGlyphMask(0x25a2, mask));
    TEST_ASSERT_FALSE(app::terminalGlyphMask(0x25ff, mask));
    const uint8_t replacement[5] = {0x1c, 0x22, 0x49, 0x22, 0x1c};
    assertGlyphMask(app::normalizeTerminalCodePoint(0x25a2), replacement);
}

void testGlyphKindsAndRenderableCoverage() {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::TerminalGlyphKind::Builtin),
                            static_cast<uint8_t>(app::terminalGlyphKind('A')));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::TerminalGlyphKind::Arrow),
                            static_cast<uint8_t>(app::terminalGlyphKind(0x2192)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::TerminalGlyphKind::BoxDrawing),
                            static_cast<uint8_t>(app::terminalGlyphKind(0x2500)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::TerminalGlyphKind::Block),
                            static_cast<uint8_t>(app::terminalGlyphKind(0x2588)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::TerminalGlyphKind::Geometric),
                            static_cast<uint8_t>(app::terminalGlyphKind(0x25cf)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::TerminalGlyphKind::Braille),
                            static_cast<uint8_t>(app::terminalGlyphKind(0x2801)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::TerminalGlyphKind::Replacement),
                            static_cast<uint8_t>(app::terminalGlyphKind(0xfffd)));

    uint8_t columns[5]{};
    TEST_ASSERT_FALSE(app::terminalGlyphMask(0x2500, columns));

    for (uint32_t codePoint = 0; codePoint <= 0xffff; ++codePoint) {
        const uint16_t normalized = app::normalizeTerminalCodePoint(codePoint);
        const app::TerminalGlyphKind kind = app::terminalGlyphKind(normalized);
        const bool algorithmic = kind == app::TerminalGlyphKind::BoxDrawing ||
                                 kind == app::TerminalGlyphKind::Block ||
                                 kind == app::TerminalGlyphKind::Braille;
        const bool fontBacked = kind == app::TerminalGlyphKind::Builtin;
        const bool maskBacked = app::terminalGlyphMask(normalized, columns);
        TEST_ASSERT_TRUE(fontBacked || maskBacked || algorithmic);
    }
}

void testXtermPaletteAndQuantization() {
    TEST_ASSERT_EQUAL_HEX16(0xf800, app::xtermColorRgb565(196));
    TEST_ASSERT_EQUAL_HEX16(0x07e0, app::xtermColorRgb565(46));
    TEST_ASSERT_EQUAL_HEX16(0x001f, app::xtermColorRgb565(21));
    TEST_ASSERT_EQUAL_UINT8(196, app::nearestXtermColor(255, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(46, app::nearestXtermColor(0, 255, 0));
    TEST_ASSERT_EQUAL_UINT8(21, app::nearestXtermColor(0, 0, 255));
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testFixedGeometryAndCellStorage);
    RUN_TEST(testSupportedGlyphRangesAndReplacement);
    RUN_TEST(testLatin1UsesDedicatedUnicodeMasks);
    RUN_TEST(testLatin1CasePairsRemainDistinct);
    RUN_TEST(testAccentedLowercaseIUsesDotlessBase);
    RUN_TEST(testEveryAcceptedArrowHasItsExactMask);
    RUN_TEST(testOnlyImplementedGeometriesAreAccepted);
    RUN_TEST(testGlyphKindsAndRenderableCoverage);
    RUN_TEST(testXtermPaletteAndQuantization);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runTests();
}
void loop() {}
#else
int main(int, char**) {
    return runTests();
}
#endif
