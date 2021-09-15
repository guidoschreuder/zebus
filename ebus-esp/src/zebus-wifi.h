#pragma once

#ifndef ESP32
  #define ESP32
#endif

const char* generate_ap_password();

void wiFiLoop(void *pvParameter);
