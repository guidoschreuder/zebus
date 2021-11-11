#ifndef ZEBUS_DATATYPES_H
#define ZEBUS_DATATYPES_H

#include <stdint.h>
#include <string.h>

#include "espnow-types.h"

struct measurement {
  uint64_t timestamp = 0;
  bool valid() { return timestamp != 0; };
  virtual void set_value(void* val);
  virtual bool value_equal(void* val);
};

struct measurement_float : measurement {
  float value;
  inline void set_value(void* val) {
    value = *(float*) val;
  }
  inline bool value_equal(void* val) {
    return (value == *(float*) val);
  }
};

struct measurement_bool : measurement  {
  bool value;
  inline void set_value(void* val) {
    value = *(bool*) val;
  }
  inline bool value_equal(void* val) {
    return (value == *(bool*) val);
  }
};

struct measurement_temperature_sensor : measurement {
  espnow_msg_temperature_sensor value;
  inline void set_value(void* val) {
    value = *(espnow_msg_temperature_sensor*) val;
  }
  inline bool value_equal(void* val) {
    espnow_msg_temperature_sensor that = *(espnow_msg_temperature_sensor*) val;
    return (value.supplyVoltage == that.supplyVoltage && value.temperatureC == that.temperatureC && strcmp(value.location, that.location) == 0);
  }
};

#endif
