#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "KeyboardLayout.h"

void setUp() {}
void tearDown() {}

namespace {

void testDocumentedLayerExamples() {
    TEST_ASSERT_EQUAL_CHAR('1', app::keyMeaning(app::keyId(0, 1)).base);
    TEST_ASSERT_EQUAL_CHAR('!', app::keyMeaning(app::keyId(0, 1)).aa);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::LayerValue::F1),
                            static_cast<uint8_t>(app::keyMeaning(app::keyId(0, 1)).fn));
    TEST_ASSERT_EQUAL_CHAR(':', app::keyMeaning(app::keyId(2, 11)).aa);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::LayerValue::Up),
                            static_cast<uint8_t>(app::keyMeaning(app::keyId(2, 11)).fn));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::LayerValue::Right),
                            static_cast<uint8_t>(app::keyMeaning(app::keyId(3, 12)).fn));
}

void testAllPhysicalCoordinatesHaveOneBaseRole() {
    for (app::PhysicalKeyId id = 0; id < app::kPhysicalKeyCount; ++id) {
        const app::KeyMeaning& meaning = app::keyMeaning(id);
        TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(app::LayerValue::None),
                              static_cast<uint8_t>(meaning.role));
        if (meaning.role == app::LayerValue::Printable) {
            TEST_ASSERT_NOT_EQUAL(0, meaning.base);
            TEST_ASSERT_NOT_EQUAL(0, meaning.aa);
        }
    }
}

void testModifiersAndSpecialLegendsUsePublishedCoordinates() {
    TEST_ASSERT_TRUE(app::isModifierKey(app::kKeyFn));
    TEST_ASSERT_TRUE(app::isModifierKey(app::kKeyAa));
    TEST_ASSERT_TRUE(app::isModifierKey(app::kKeyCtrl));
    TEST_ASSERT_TRUE(app::isModifierKey(app::kKeyOpt));
    TEST_ASSERT_TRUE(app::isModifierKey(app::kKeyAlt));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::LayerValue::Escape),
                            static_cast<uint8_t>(app::keyMeaning(app::kKeyBacktick).fn));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::LayerValue::DeleteForward),
                            static_cast<uint8_t>(app::keyMeaning(app::kKeyDelete).fn));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(app::LayerValue::Enter),
                            static_cast<uint8_t>(app::keyMeaning(app::kKeyEnter).role));
}

}  // namespace

int runTests() {
    UNITY_BEGIN();
    RUN_TEST(testDocumentedLayerExamples);
    RUN_TEST(testAllPhysicalCoordinatesHaveOneBaseRole);
    RUN_TEST(testModifiersAndSpecialLegendsUsePublishedCoordinates);
    return UNITY_END();
}

#ifdef ARDUINO
void setup() { delay(2000); runTests(); }
void loop() {}
#else
int main(int, char**) { return runTests(); }
#endif
