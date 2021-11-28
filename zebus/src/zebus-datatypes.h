#ifndef ZEBUS_DATATYPES_H
#define ZEBUS_DATATYPES_H

#include <stdint.h>

#include "espnow-types.h"

struct measurement {
  uint64_t timestamp = 0;
};

struct measurement_float : measurement {
  float value;
};

struct measurement_bool : measurement  {
  bool value;
};

struct measurement_temperature_sensor : measurement {
  espnow_msg_temperature_sensor value;
};

bool measurement_valid(measurement& measurement);

#endif
