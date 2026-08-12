#include <unity.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <cstring>
#include "Diagnostics.h"
void setUp(){} void tearDown(){}
void testDiagnosticsAreBoundedAndContainNoCredentialFields(){app::DiagnosticsSnapshot s{};s.scanNamed=13;s.scanHidden=2;s.aaLocked=true;char line[64]{};TEST_ASSERT_TRUE(app::formatDiagnosticLine(s,0,line,sizeof(line)));TEST_ASSERT_NOT_NULL(std::strstr(line,"13"));TEST_ASSERT_NULL(std::strstr(line,"password"));for(size_t i=0;i<app::kDiagnosticLineCount;++i)TEST_ASSERT_TRUE(app::formatDiagnosticLine(s,i,line,sizeof(line)));TEST_ASSERT_FALSE(app::formatDiagnosticLine(s,app::kDiagnosticLineCount,line,sizeof(line)));}
int runTests(){UNITY_BEGIN();RUN_TEST(testDiagnosticsAreBoundedAndContainNoCredentialFields);return UNITY_END();}
#ifdef ARDUINO
void setup(){delay(2000);runTests();} void loop(){}
#else
int main(int,char**){return runTests();}
#endif
