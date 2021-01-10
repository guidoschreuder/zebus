
#if defined(ARDUINO) && !defined(UNIT_TEST)

#include <Arduino.h>

void setup() {
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
}

#else
// keep `pio run` happy
int main() {
  return 0;
}
#endif
