#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "ScrollbackViewport.h"

void setUp() {}
void tearDown() {}

using app::ScrollbackViewport;

namespace {

void test_line_and_page_scroll_saturate() {
    ScrollbackViewport viewport;
    viewport.scrollLines(1, 20, 10);
    TEST_ASSERT_EQUAL_UINT(1, viewport.distanceFromBottom());
    viewport.scrollPages(1, 20, 10);
    TEST_ASSERT_EQUAL_UINT(10, viewport.distanceFromBottom());
    viewport.scrollPages(-1, 20, 10);
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());
}

void test_short_buffer_cannot_scroll() {
    ScrollbackViewport viewport;
    viewport.scrollLines(10, 5, 10);
    TEST_ASSERT_FALSE(viewport.isScrolled());
}

void test_append_preserves_scrolled_content() {
    ScrollbackViewport viewport;
    viewport.scrollLines(3, 20, 10);
    viewport.onLinesAppended(2, 22, 10);
    TEST_ASSERT_EQUAL_UINT(5, viewport.distanceFromBottom());
}

void test_append_at_bottom_stays_at_bottom() {
    ScrollbackViewport viewport;
    viewport.onLinesAppended(5, 25, 10);
    TEST_ASSERT_EQUAL_UINT(0, viewport.distanceFromBottom());
}

void test_eviction_clamps_oldest_view() {
    ScrollbackViewport viewport;
    viewport.scrollLines(100, 20, 10);
    viewport.onLinesAppended(3, 20, 10);
    TEST_ASSERT_EQUAL_UINT(10, viewport.distanceFromBottom());
    viewport.jumpToBottom();
    TEST_ASSERT_FALSE(viewport.isScrolled());
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_line_and_page_scroll_saturate);
    RUN_TEST(test_short_buffer_cannot_scroll);
    RUN_TEST(test_append_preserves_scrolled_content);
    RUN_TEST(test_append_at_bottom_stays_at_bottom);
    RUN_TEST(test_eviction_clamps_oldest_view);
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
