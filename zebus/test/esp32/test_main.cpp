// test is just an incomplete stub for now

#if defined(UNIT_TEST)

#include "unity.h"

extern "C" {
void app_main();
}

void app_main() {
  //delay(2000);
  UNITY_BEGIN();

  // calls to tests will go here

  UNITY_END();
}

#endif