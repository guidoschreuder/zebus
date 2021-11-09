#ifndef _ZEBUS_SYSTEM_INFO_H
#define _ZEBUS_SYSTEM_INFO_H

#include "zebus-messages.h"
#include "espnow-types.h"

#define MAX_SENSORS 8

#define WIFI_NO_SIGNAL 0x80000000
#define OUTSIDE_TEMP_FALLBACK 10.0;

#define MEASUREMENT(DATATYPE)               \
  uint64_t timestamp = 0;                   \
  bool valid() { return timestamp != 0; };  \
  DATATYPE value;

struct measurement_float {
  MEASUREMENT(float)
};

struct measurement_bool {
  MEASUREMENT(bool)
};

struct measurement_temperature_sensor {
  MEASUREMENT(espnow_msg_temperature_sensor)
};

struct system_info_t {
  struct ebus {
    identification_t self_id;
    identification_t heater_id;
    uint8_t queue_size;
  } ebus;
  struct wifi {
    struct config_ap {
      const char* ap_name = NULL;
      const char* ap_password = NULL;
      bool active;
    } config_ap;
    uint32_t ip_addr;
    int32_t rssi;
  } wifi;
  struct ntp {
    uint64_t last_init;
  } ntp;
  struct heater {
    measurement_float max_flow_setpoint;
    measurement_float flow_temp;
    measurement_float return_temp;
    measurement_bool flame;
    measurement_float flow;
    measurement_float modulation;
  } heater;
  measurement_temperature_sensor sensors[MAX_SENSORS];
  uint8_t num_sensors = 0;
};

extern struct system_info_t* system_info;

#endif
