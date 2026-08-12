#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <cstring>

#include "TextEditModel.h"

void setUp() {}
void tearDown() {}

using app::TextEditModel;

namespace {

void test_insert_move_and_delete() {
    char storage[8] = "ac";
    TextEditModel edit(storage, sizeof(storage));
    TEST_ASSERT_TRUE(edit.moveLeft());
    TEST_ASSERT_TRUE(edit.insert('b'));
    TEST_ASSERT_EQUAL_STRING("abc", edit.data());
    TEST_ASSERT_EQUAL_UINT(2, edit.cursor());
    TEST_ASSERT_TRUE(edit.eraseBeforeCursor());
    TEST_ASSERT_EQUAL_STRING("ac", edit.data());
    TEST_ASSERT_TRUE(edit.eraseAtCursor());
    TEST_ASSERT_EQUAL_STRING("a", edit.data());
}

void test_boundaries_and_capacity() {
    char storage[4] = "abc";
    TextEditModel edit(storage, sizeof(storage));
    TEST_ASSERT_FALSE(edit.insert('d'));
    TEST_ASSERT_FALSE(edit.moveRight());
    TEST_ASSERT_TRUE(edit.moveLeft());
    TEST_ASSERT_TRUE(edit.moveLeft());
    TEST_ASSERT_TRUE(edit.moveLeft());
    TEST_ASSERT_FALSE(edit.moveLeft());
    TEST_ASSERT_FALSE(edit.eraseBeforeCursor());
    TEST_ASSERT_EQUAL_CHAR('\0', storage[edit.length()]);
}

void test_assign_is_transactional_when_too_long() {
    char storage[5] = "old";
    TextEditModel edit(storage, sizeof(storage));
    TEST_ASSERT_FALSE(edit.assign("12345"));
    TEST_ASSERT_EQUAL_STRING("old", storage);
    TEST_ASSERT_TRUE(edit.assign("new"));
    TEST_ASSERT_EQUAL_STRING("new", storage);
}

void test_visible_mask_and_cursor() {
    char storage[16] = "password";
    TextEditModel edit(storage, sizeof(storage));
    char shown[6]{};
    size_t start = 0;
    size_t cursor = 0;
    TEST_ASSERT_EQUAL_UINT(5, edit.visibleSlice(shown, sizeof(shown), 5, true, start, cursor));
    TEST_ASSERT_EQUAL_STRING("*****", shown);
    TEST_ASSERT_EQUAL_UINT(3, start);
    TEST_ASSERT_EQUAL_UINT(5, cursor);
}

void test_sensitive_clear_overwrites_storage() {
    char storage[8] = "secret";
    TextEditModel edit(storage, sizeof(storage));
    edit.clearSensitive();
    TEST_ASSERT_EQUAL_UINT(0, edit.length());
    for (char value : storage) {
        TEST_ASSERT_EQUAL_CHAR('\0', value);
    }
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_insert_move_and_delete);
    RUN_TEST(test_boundaries_and_capacity);
    RUN_TEST(test_assign_is_transactional_when_too_long);
    RUN_TEST(test_visible_mask_and_cursor);
    RUN_TEST(test_sensitive_clear_overwrites_storage);
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
