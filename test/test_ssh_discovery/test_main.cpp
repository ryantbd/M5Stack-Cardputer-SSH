#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include "SshDiscovery.h"
void setUp(){} void tearDown(){}
void testDiscoveryListNormalizesAndDeduplicates(){
    app::SshDiscoveryList list;
    TEST_ASSERT_TRUE(list.add("first","server",0));
    TEST_ASSERT_FALSE(list.add("dup","server.local",22));
    TEST_ASSERT_TRUE(list.add("other","server.local",2222));
    TEST_ASSERT_EQUAL_UINT32(2,list.count());
    TEST_ASSERT_EQUAL_STRING("server.local",list.results()[0].target);
    TEST_ASSERT_EQUAL_UINT16(22,list.results()[0].port);
}
int runTests(){UNITY_BEGIN();RUN_TEST(testDiscoveryListNormalizesAndDeduplicates);return UNITY_END();}
#ifdef ARDUINO
void setup(){delay(2000);runTests();} void loop(){}
#else
int main(int,char**){return runTests();}
#endif
