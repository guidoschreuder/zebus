#ifndef _ZEBUS_SYSTEM_INFO_H
#define _ZEBUS_SYSTEM_INFO_H

#include "zebus-datatypes.h"
#include "zebus-messages.h"

#define WIFI_NO_SIGNAL 0x80000000
#define OUTSIDE_TEMP_FALLBACK 10.0;

struct system_info_t {
  struct ebus {
    identification_t self_id;
    identification_t heater_id;
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
};

extern struct system_info_t* system_info;

#endif
