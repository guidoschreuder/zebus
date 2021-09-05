#pragma once

#ifndef ESP32
  #define ESP32
#endif

#include "system-info.h"
#include "WiFiManager.h"
#include "nvs_flash.h"
#include "config.h"

const char* generate_ap_password();

void setupWiFiAndKeepAlive(void *pvParameter);
