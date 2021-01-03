#if defined(ARDUINO) && defined(UNIT_TEST)

#include <Arduino.h>
#include "unity.h"

void setup() {
  //delay(2000);
  UNITY_BEGIN();

  // calls to tests will go here

  UNITY_END();
}

void loop() {

}

#endif