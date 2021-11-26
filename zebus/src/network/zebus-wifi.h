#ifndef ZEBUS_WIFI_H
#define ZEBUS_WIFI_H

#include "zebus-log.h"

#ifndef ESP32
  #define ESP32
#endif

const char* generate_ap_password();

void wiFiTask(void *pvParameter);

#endif
