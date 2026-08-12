#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "TerminalKeyEncoder.h"

void setUp() {}
void tearDown() {}

using app::KeyEvent;
using app::KeyEventType;
using app::TerminalKeyEncoder;

namespace {

void assertEncoding(KeyEventType type, uint8_t value, const uint8_t* expected,
                    size_t length,
                    const app::TerminalInputModes& modes = {}) {
    KeyEvent event;
    event.type = type;
    event.value = value;
    const app::EncodedKey encoded = TerminalKeyEncoder::encode(event, modes);
    TEST_ASSERT_EQUAL_UINT(length, encoded.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, encoded.bytes, length);
}

void test_printable_and_control_bytes() {
    const uint8_t text[] = {'x'};
    assertEncoding(KeyEventType::Text, 'x', text, sizeof(text));
    const uint8_t control[] = {3};
    assertEncoding(KeyEventType::Control, 3, control, sizeof(control));
    const uint8_t nul[] = {0};
    assertEncoding(KeyEventType::Control, 0, nul, sizeof(nul));
    const uint8_t alt[] = {0x1b, ':'};
    assertEncoding(KeyEventType::AltText, ':', alt, sizeof(alt));
}

void test_function_keys_use_xterm_sequences() {
    const uint8_t f1[] = {0x1b, 'O', 'P'};
    const uint8_t f4[] = {0x1b, 'O', 'S'};
    const uint8_t f5[] = {0x1b, '[', '1', '5', '~'};
    const uint8_t f12[] = {0x1b, '[', '2', '4', '~'};
    assertEncoding(KeyEventType::Function, 1, f1, sizeof(f1));
    assertEncoding(KeyEventType::Function, 4, f4, sizeof(f4));
    assertEncoding(KeyEventType::Function, 5, f5, sizeof(f5));
    assertEncoding(KeyEventType::Function, 12, f12, sizeof(f12));
    KeyEvent invalid;
    invalid.type = KeyEventType::Function;
    invalid.value = 0;
    TEST_ASSERT_EQUAL_UINT(
        0, TerminalKeyEncoder::encode(invalid, app::TerminalInputModes{}).length);
    invalid.value = 13;
    TEST_ASSERT_EQUAL_UINT(
        0, TerminalKeyEncoder::encode(invalid, app::TerminalInputModes{}).length);
}

void test_terminal_special_keys() {
    const uint8_t enter[] = {0x0d};
    assertEncoding(KeyEventType::Confirm, 0, enter, sizeof(enter));
    const uint8_t tab[] = {0x09};
    assertEncoding(KeyEventType::Tab, 0, tab, sizeof(tab));
    const uint8_t backspace[] = {0x7f};
    assertEncoding(KeyEventType::DeleteBackward, 0, backspace, sizeof(backspace));
    const uint8_t deletion[] = {0x1b, '[', '3', '~'};
    assertEncoding(KeyEventType::DeleteForward, 0, deletion, sizeof(deletion));
    const uint8_t escape[] = {0x1b};
    assertEncoding(KeyEventType::Escape, 0, escape, sizeof(escape));
}

void test_arrow_sequences() {
    const uint8_t up[] = {0x1b, '[', 'A'};
    const uint8_t down[] = {0x1b, '[', 'B'};
    const uint8_t right[] = {0x1b, '[', 'C'};
    const uint8_t left[] = {0x1b, '[', 'D'};
    assertEncoding(KeyEventType::MoveUp, 0, up, sizeof(up));
    assertEncoding(KeyEventType::MoveDown, 0, down, sizeof(down));
    assertEncoding(KeyEventType::MoveRight, 0, right, sizeof(right));
    assertEncoding(KeyEventType::MoveLeft, 0, left, sizeof(left));
}

void test_application_cursor_sequences() {
    app::TerminalInputModes modes;
    modes.applicationCursor = true;
    const uint8_t up[] = {0x1b, 'O', 'A'};
    const uint8_t down[] = {0x1b, 'O', 'B'};
    const uint8_t right[] = {0x1b, 'O', 'C'};
    const uint8_t left[] = {0x1b, 'O', 'D'};
    assertEncoding(KeyEventType::MoveUp, 0, up, sizeof(up), modes);
    assertEncoding(KeyEventType::MoveDown, 0, down, sizeof(down), modes);
    assertEncoding(KeyEventType::MoveRight, 0, right, sizeof(right), modes);
    assertEncoding(KeyEventType::MoveLeft, 0, left, sizeof(left), modes);
}

void test_global_and_local_events_encode_nothing() {
    for (KeyEventType type : {KeyEventType::ToggleTerminalMode,
                              KeyEventType::OpenSettings,
                              KeyEventType::EndSession,
                              KeyEventType::Back,
                              KeyEventType::JumpBottom,
                              KeyEventType::None}) {
        KeyEvent event;
        event.type = type;
        TEST_ASSERT_EQUAL_UINT(
            0, TerminalKeyEncoder::encode(event, app::TerminalInputModes{}).length);
    }
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_printable_and_control_bytes);
    RUN_TEST(test_terminal_special_keys);
    RUN_TEST(test_function_keys_use_xterm_sequences);
    RUN_TEST(test_arrow_sequences);
    RUN_TEST(test_application_cursor_sequences);
    RUN_TEST(test_global_and_local_events_encode_nothing);
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
