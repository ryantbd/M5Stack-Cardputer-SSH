#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include "SessionFlow.h"
void setUp(){} void tearDown(){}
app::SessionFlow live(){app::SessionFlow f;f.apply(app::SessionEvent::ConfigReady);f.apply(app::SessionEvent::WifiConnected);f.apply(app::SessionEvent::HostSelected);f.apply(app::SessionEvent::HostVerified);f.apply(app::SessionEvent::Authenticated);return f;}
void testWifiOpensLauncherWithoutSsh(){app::SessionFlow f;f.apply(app::SessionEvent::ConfigReady);TEST_ASSERT_TRUE(f.apply(app::SessionEvent::WifiConnected));TEST_ASSERT_EQUAL_UINT8((uint8_t)app::AppPage::HostLauncher,(uint8_t)f.page());TEST_ASSERT_FALSE(f.sessionLive());}
void testLocalBlocksWritesAndEndKeepsWifi(){auto f=live();TEST_ASSERT_TRUE(f.sshWriteAllowed());f.apply(app::SessionEvent::ToggleTerminalMode);TEST_ASSERT_FALSE(f.sshWriteAllowed());f.apply(app::SessionEvent::RequestEnd);f.apply(app::SessionEvent::ConfirmEnd);TEST_ASSERT_TRUE(f.wifiReady());TEST_ASSERT_FALSE(f.sessionLive());TEST_ASSERT_EQUAL_UINT8((uint8_t)app::AppPage::HostLauncher,(uint8_t)f.page());}
int runTests(){UNITY_BEGIN();RUN_TEST(testWifiOpensLauncherWithoutSsh);RUN_TEST(testLocalBlocksWritesAndEndKeepsWifi);return UNITY_END();}
#ifdef ARDUINO
void setup(){delay(2000);runTests();} void loop(){}
#else
int main(int,char**){return runTests();}
#endif
