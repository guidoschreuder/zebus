#ifndef _ZEBUS_WIFI
#define _ZEBUS_WIFI

#ifndef ESP32
  #define ESP32
#endif

const char* generate_ap_password();

void wiFiLoop(void *pvParameter);

#endif
