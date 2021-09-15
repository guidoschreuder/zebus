#pragma once

#include "zebus-messages.h"

#define WIFI_NO_SIGNAL 0x80000000

struct system_info_t {
  struct ebus {
    identification_t self_id;
    identification_t heater_id;
    uint8_t queue_size;
    bool flame = false;
    uint16_t flow;
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
};

extern struct system_info_t* system_info;
