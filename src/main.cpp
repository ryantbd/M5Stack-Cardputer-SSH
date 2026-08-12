#include <Arduino.h>

#include "AppController.h"

SET_LOOP_TASK_STACK_SIZE(24 * 1024);

#ifndef PIO_UNIT_TESTING
namespace {

app::AppController application;

}  // namespace

void setup() {
    application.begin();
}

void loop() {
    application.loop();
}
#endif
