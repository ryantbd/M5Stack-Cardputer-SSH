#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "ListViewport.h"

void setUp() {}
void tearDown() {}

void test_empty_list_has_no_rows() {
    const app::ListViewportState state = app::listViewport(0, 0, 0);
    TEST_ASSERT_EQUAL_UINT32(0, state.first);
    TEST_ASSERT_EQUAL_UINT32(0, state.rows);
}

void test_short_lists_do_not_duplicate_items() {
    app::ListViewportState state = app::listViewport(1, 0, 0);
    TEST_ASSERT_EQUAL_UINT32(0, state.first);
    TEST_ASSERT_EQUAL_UINT32(1, state.rows);
    state = app::listViewport(2, 1, 0);
    TEST_ASSERT_EQUAL_UINT32(0, state.first);
    TEST_ASSERT_EQUAL_UINT32(2, state.rows);
}

void test_highlight_moves_before_window_scrolls() {
    TEST_ASSERT_EQUAL_UINT32(0, app::listViewport(6, 0, 0).first);
    TEST_ASSERT_EQUAL_UINT32(0, app::listViewport(6, 1, 0).first);
    TEST_ASSERT_EQUAL_UINT32(0, app::listViewport(6, 2, 0).first);
    TEST_ASSERT_EQUAL_UINT32(1, app::listViewport(6, 3, 0).first);
    TEST_ASSERT_EQUAL_UINT32(2, app::listViewport(6, 4, 1).first);
}

void test_final_selection_uses_final_row() {
    const app::ListViewportState state = app::listViewport(6, 5, 0);
    TEST_ASSERT_EQUAL_UINT32(3, state.first);
    TEST_ASSERT_EQUAL_UINT32(3, state.rows);
}

void test_wrap_uses_first_and_final_windows() {
    TEST_ASSERT_EQUAL_UINT32(3, app::listViewport(6, 5, 0).first);
    TEST_ASSERT_EQUAL_UINT32(0, app::listViewport(6, 0, 3).first);
}

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_list_has_no_rows);
    RUN_TEST(test_short_lists_do_not_duplicate_items);
    RUN_TEST(test_highlight_moves_before_window_scrolls);
    RUN_TEST(test_final_selection_uses_final_row);
    RUN_TEST(test_wrap_uses_first_and_final_windows);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() { runTests(); }
void loop() {}
#else
int main(int, char**) { return runTests(); }
#endif
