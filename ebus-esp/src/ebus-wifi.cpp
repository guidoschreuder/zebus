#include "ebus-wifi.h"
#include "Arduino.h"

const char PWD_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

const char* generate_ap_password() {
  static char pwd[EBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH + 1] = {0};
  for (uint8_t i = 0; i < EBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH; i++) {
    pwd[i] = PWD_CHARS[random(sizeof(PWD_CHARS))];
  }
  return pwd;
}

void setupWiFiAndKeepAlive(void *pvParameter) {

  // CRITICAL: WiFi will fail to startup without this
  nvs_flash_init();

  WiFi.mode(WIFI_STA); // explicitly set mode, ESP32 defaults to STA+AP

  // TODO: generate a password to use and show it on screen
  // printf("pwd %s\n", generate_ap_password());

  WiFiManager wiFiManager;
  if (!wiFiManager.autoConnect(EBUS_WIFI_CONFIG_AP_NAME, "hollebolle")) {
    printf("WiFi manager failed!\n");
    // TODO: figure out what to do now
  }

  while(1) {
    if (WiFi.status() == WL_CONNECTED) {
      //system_info->wifi.rssi = WiFi.RSSI();
      printf("WiFi connection is good, RSSI: %d\n", WiFi.RSSI());
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    printf("WiFi connection was lost\n");
    //system_info->wifi.rssi = 0XFFFFFFFF;
    if (WiFi.reconnect()) {
      printf("WiFi connection restored\n");
    }
  }

}
