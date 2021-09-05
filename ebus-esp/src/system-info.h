#pragma once

#include "ebus-messages.h"

#define WIFI_NO_SIGNAL 0x80000000

struct system_info_t {
  identification_t self_id;
  identification_t heater_id;
  bool flame = false;
  uint16_t flow;
  struct wifi {
    struct config_ap {
      const char* ap_name;
      const char* ap_password;
      bool active;
    } config_ap;
    int32_t rssi;
  } wifi;
};

extern struct system_info_t* system_info;
