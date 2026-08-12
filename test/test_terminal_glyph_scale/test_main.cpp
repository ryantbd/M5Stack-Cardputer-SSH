#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "TerminalGlyphScale.h"

void setUp() {}
void tearDown() {}

namespace {

void assertSpan(uint8_t expectedOffset, uint8_t expectedLength,
                app::TerminalGlyphSpan actual) {
    TEST_ASSERT_EQUAL_UINT8(expectedOffset, actual.offset);
    TEST_ASSERT_EQUAL_UINT8(expectedLength, actual.length);
}

void test_identity_and_large_cell_pixel_spans() {
    assertSpan(2, 1, app::scaleTerminalGlyphSpan(2, 1, 5, 5));
    assertSpan(2, 2, app::scaleTerminalGlyphSpan(2, 1, 5, 6));
    assertSpan(5, 1, app::scaleTerminalGlyphSpan(4, 1, 5, 6));
    assertSpan(3, 2, app::scaleTerminalGlyphSpan(3, 1, 8, 9));
    assertSpan(8, 1, app::scaleTerminalGlyphSpan(7, 1, 8, 9));
}

void test_rectangles_fill_target_and_clip_at_source_edge() {
    assertSpan(0, 6, app::scaleTerminalGlyphSpan(0, 5, 5, 6));
    assertSpan(0, 9, app::scaleTerminalGlyphSpan(0, 8, 8, 9));
    assertSpan(4, 2, app::scaleTerminalGlyphSpan(3, 9, 5, 6));
}

void test_invalid_spans_are_empty() {
    assertSpan(0, 0, app::scaleTerminalGlyphSpan(0, 0, 5, 6));
    assertSpan(0, 0, app::scaleTerminalGlyphSpan(5, 1, 5, 6));
    assertSpan(0, 0, app::scaleTerminalGlyphSpan(0, 1, 0, 6));
    assertSpan(0, 0, app::scaleTerminalGlyphSpan(0, 1, 5, 0));
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_identity_and_large_cell_pixel_spans);
    RUN_TEST(test_rectangles_fill_target_and_clip_at_source_edge);
    RUN_TEST(test_invalid_spans_are_empty);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    runTests();
}
void loop() {}
#else
int main(int, char**) { return runTests(); }
#endif
