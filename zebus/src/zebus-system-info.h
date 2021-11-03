#ifndef _ZEBUS_SYSTEM_INFO_H
#define _ZEBUS_SYSTEM_INFO_H

#include "zebus-messages.h"
#include "espnow-types.h"

#define MAX_SENSORS 8

#define WIFI_NO_SIGNAL 0x80000000
#define INVALID_TEMP -255.0

#define OUTSIDE_TEMP_FALLBACK 10.0;

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
    long last_init;
  } ntp;
  struct heater {
    float max_flow_setpoint = INVALID_TEMP;
    float flow_temp = INVALID_TEMP;
    float return_temp = INVALID_TEMP;
    bool flame = false;
    float flow;
    float modulation;
  } heater;
  espnow_msg_temperature_sensor sensors[MAX_SENSORS];
  uint8_t num_sensors = 0;
};

extern struct system_info_t* system_info;

#endif
