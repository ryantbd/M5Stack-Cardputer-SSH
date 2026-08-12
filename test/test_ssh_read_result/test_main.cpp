#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <initializer_list>

#include "SshReadResult.h"

void setUp() {}
void tearDown() {}

namespace {

void test_read_result_classifies_data_and_idle() {
    const app::SshReadResult data =
        app::classifySshReadResult(37, 256, -2, -127);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(app::SshReadStatus::Data),
        static_cast<uint8_t>(data.status));
    TEST_ASSERT_EQUAL_UINT(37, data.length);

    for (const int raw : {0, -2}) {
        const app::SshReadResult idle =
            app::classifySshReadResult(raw, 256, -2, -127);
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(app::SshReadStatus::WouldBlock),
            static_cast<uint8_t>(idle.status));
        TEST_ASSERT_EQUAL_UINT(0, idle.length);
        TEST_ASSERT_FALSE(app::sshReadTerminatesSession(idle.status));
    }
}

void test_read_error_and_eof_terminate_the_session() {
    const app::SshReadResult error =
        app::classifySshReadResult(-1, 256, -2, -127);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(app::SshReadStatus::Error),
        static_cast<uint8_t>(error.status));
    TEST_ASSERT_TRUE(app::sshReadTerminatesSession(error.status));

    const app::SshReadResult eof =
        app::classifySshReadResult(-127, 256, -2, -127);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(app::SshReadStatus::Closed),
        static_cast<uint8_t>(eof.status));
    TEST_ASSERT_TRUE(app::sshReadTerminatesSession(eof.status));

    const app::SshReadResult unexpected =
        app::classifySshReadResult(-9, 256, -2, -127);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(app::SshReadStatus::Error),
        static_cast<uint8_t>(unexpected.status));
    TEST_ASSERT_TRUE(app::sshReadTerminatesSession(unexpected.status));

    const app::SshReadResult oversized =
        app::classifySshReadResult(257, 256, -2, -127);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(app::SshReadStatus::Error),
        static_cast<uint8_t>(oversized.status));
    TEST_ASSERT_TRUE(app::sshReadTerminatesSession(oversized.status));
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(test_read_result_classifies_data_and_idle);
    RUN_TEST(test_read_error_and_eof_terminate_the_session);
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
