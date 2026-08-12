#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <initializer_list>

#include "KeyboardRouter.h"

void setUp() {}
void tearDown() {}

namespace {

app::KeyboardSnapshot snapshot(std::initializer_list<app::PhysicalKeyId> ids) {
    app::KeyboardSnapshot result;
    for (const app::PhysicalKeyId id : ids) result.set(id, true);
    return result;
}

app::KeyEvent take(app::KeyboardRouter& router) {
    app::KeyEvent result;
    TEST_ASSERT_TRUE(router.pop(result));
    return result;
}

void assertEvent(app::KeyboardRouter& router, app::KeyEventType type, bool repeated = false) {
    const app::KeyEvent result = take(router);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type), static_cast<uint8_t>(result.type));
    TEST_ASSERT_EQUAL(repeated, result.repeat);
}

void assertText(app::KeyboardRouter& router, char expected, app::KeyEventType type = app::KeyEventType::Text) {
    const app::KeyEvent result = take(router);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type), static_cast<uint8_t>(result.type));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), result.value);
}

void assertNoEvent(app::KeyboardRouter& router) {
    app::KeyEvent result;
    TEST_ASSERT_FALSE(router.pop(result));
}

void testHeldFnRoutesSuccessiveDirectionPresses() {
    app::KeyboardRouter router;
    auto state = snapshot({app::kKeyFn});
    router.update(state, app::InputContext::Menu, 0);
    state.press(app::kKeyUpLegend);
    router.update(state, app::InputContext::Menu, 10);
    assertEvent(router, app::KeyEventType::MoveUp);
    state.release(app::kKeyUpLegend);
    router.update(state, app::InputContext::Menu, 20);
    state.press(app::kKeyDownLegend);
    router.update(state, app::InputContext::Menu, 30);
    assertEvent(router, app::KeyEventType::MoveDown);
}

void testIndependentEdgesInOneSnapshotAreQueued() {
    app::KeyboardRouter router;
    router.update(snapshot({app::keyId(2, 2), app::keyId(2, 3)}), app::InputContext::TextInput, 0);
    assertText(router, 'a');
    assertText(router, 's');
}

void testDirectionRepeatsAt350Then150Milliseconds() {
    app::KeyboardRouter router;
    const auto held = snapshot({app::kKeyFn, app::kKeyUpLegend});
    router.update(held, app::InputContext::Menu, 0);
    assertEvent(router, app::KeyEventType::MoveUp, false);
    router.update(held, app::InputContext::Menu, 349);
    assertNoEvent(router);
    router.update(held, app::InputContext::Menu, 350);
    assertEvent(router, app::KeyEventType::MoveUp, true);
    router.update(held, app::InputContext::Menu, 500);
    assertEvent(router, app::KeyEventType::MoveUp, true);
}

void testRemoteAndTextDeletionRepeatButConfirmationDoesNot() {
    app::KeyboardRouter router;
    const auto deletion = snapshot({app::kKeyDelete});
    router.update(deletion, app::InputContext::RemoteTerminal, 0);
    assertEvent(router, app::KeyEventType::DeleteBackward);
    router.update(deletion, app::InputContext::RemoteTerminal, 350);
    assertEvent(router, app::KeyEventType::DeleteBackward, true);
    router.update({}, app::InputContext::RemoteTerminal, 360);
    const auto enter = snapshot({app::kKeyEnter});
    router.update(enter, app::InputContext::RemoteTerminal, 400);
    assertEvent(router, app::KeyEventType::Confirm);
    router.update(enter, app::InputContext::RemoteTerminal, 1000);
    assertNoEvent(router);
}

void testSuppressionRequiresFullReleaseAndPreservesAaLock() {
    app::KeyboardRouter router;
    router.update(snapshot({app::kKeyAa}), app::InputContext::TextInput, 0);
    router.update({}, app::InputContext::TextInput, 100);
    TEST_ASSERT_TRUE(router.aaLocked());
    router.suppressUntilReleased();
    router.update(snapshot({app::keyId(2, 2)}), app::InputContext::TextInput, 200);
    assertNoEvent(router);
    router.update({}, app::InputContext::TextInput, 210);
    router.update(snapshot({app::keyId(2, 2)}), app::InputContext::TextInput, 220);
    assertText(router, 'A');
}

void testAaTapTogglesButChordDoesNot() {
    app::KeyboardRouter router;
    router.update(snapshot({app::kKeyAa}), app::InputContext::TextInput, 0);
    router.update({}, app::InputContext::TextInput, 100);
    TEST_ASSERT_TRUE(router.aaLocked());

    router.update(snapshot({app::kKeyAa}), app::InputContext::TextInput, 200);
    router.update(snapshot({app::kKeyAa, app::keyId(0, 1)}), app::InputContext::TextInput, 220);
    assertText(router, '!');
    router.update({}, app::InputContext::TextInput, 250);
    TEST_ASSERT_TRUE(router.aaLocked());
}

void testAaLongHoldDoesNotToggleAndHeldAaRoutesSuccessiveKeys() {
    app::KeyboardRouter router;
    auto state = snapshot({app::kKeyAa});
    router.update(state, app::InputContext::TextInput, 0);
    state.press(app::keyId(2, 2));
    router.update(state, app::InputContext::TextInput, 10);
    assertText(router, 'A');
    state.release(app::keyId(2, 2));
    router.update(state, app::InputContext::TextInput, 20);
    state.press(app::keyId(2, 3));
    router.update(state, app::InputContext::TextInput, 30);
    assertText(router, 'S');
    router.update({}, app::InputContext::TextInput, 301);
    TEST_ASSERT_FALSE(router.aaLocked());
}

void testFnPrecedesAaAndCtrlAltUseResolvedCharacter() {
    app::KeyboardRouter router;
    router.update(snapshot({app::kKeyFn, app::kKeyAa, app::kKeyUpLegend}), app::InputContext::RemoteTerminal, 0);
    assertEvent(router, app::KeyEventType::MoveUp);
    router.update({}, app::InputContext::RemoteTerminal, 10);

    router.update(snapshot({app::kKeyCtrl, app::keyId(3, 5)}), app::InputContext::RemoteTerminal, 20);
    app::KeyEvent control = take(router);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::KeyEventType::Control), static_cast<uint8_t>(control.type));
    TEST_ASSERT_EQUAL_UINT8(3, control.value);
    router.update({}, app::InputContext::RemoteTerminal, 30);

    router.update(snapshot({app::kKeyAlt, app::kKeyAa, app::keyId(2, 11)}),
                  app::InputContext::RemoteTerminal, 40);
    assertText(router, ':', app::KeyEventType::AltText);
}

void testOptTerminalChordsAreConsumedAndLocalTextIsIgnored() {
    app::KeyboardRouter router;
    router.update(snapshot({app::kKeyOpt, app::kKeySpace}), app::InputContext::RemoteTerminal, 0);
    assertEvent(router, app::KeyEventType::ToggleTerminalMode);
    assertNoEvent(router);
    router.update({}, app::InputContext::RemoteTerminal, 10);
    router.update(snapshot({app::kKeyOpt, app::kKeyEnter}), app::InputContext::LocalScroll, 20);
    assertEvent(router, app::KeyEventType::OpenSettings);
    router.update({}, app::InputContext::LocalScroll, 30);
    router.update(snapshot({app::keyId(2, 2)}), app::InputContext::LocalScroll, 40);
    assertNoEvent(router);
}

void testFunctionAndExactControlSpecialValues() {
    app::KeyboardRouter router;
    router.update(snapshot({app::kKeyFn, app::keyId(0, 12)}), app::InputContext::RemoteTerminal, 0);
    app::KeyEvent function = take(router);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::KeyEventType::Function),
                            static_cast<uint8_t>(function.type));
    TEST_ASSERT_EQUAL_UINT8(12, function.value);
    router.update({}, app::InputContext::RemoteTerminal, 10);
    router.update(snapshot({app::kKeyCtrl, app::keyId(0, 7)}),
                  app::InputContext::RemoteTerminal, 20);
    app::KeyEvent control = take(router);
    TEST_ASSERT_EQUAL_UINT8(0x1f, control.value);
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testHeldFnRoutesSuccessiveDirectionPresses);
    RUN_TEST(testIndependentEdgesInOneSnapshotAreQueued);
    RUN_TEST(testDirectionRepeatsAt350Then150Milliseconds);
    RUN_TEST(testRemoteAndTextDeletionRepeatButConfirmationDoesNot);
    RUN_TEST(testSuppressionRequiresFullReleaseAndPreservesAaLock);
    RUN_TEST(testAaTapTogglesButChordDoesNot);
    RUN_TEST(testAaLongHoldDoesNotToggleAndHeldAaRoutesSuccessiveKeys);
    RUN_TEST(testFnPrecedesAaAndCtrlAltUseResolvedCharacter);
    RUN_TEST(testOptTerminalChordsAreConsumedAndLocalTextIsIgnored);
    RUN_TEST(testFunctionAndExactControlSpecialValues);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() { delay(2000); runTests(); }
void loop() {}
#else
int main(int, char**) { return runTests(); }
#endif
