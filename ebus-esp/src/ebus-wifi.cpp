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

WiFiManager wiFiManager;

void saveParamsCallback() {
  printf("WiFiManager saveParamsCallback called\n");
  system_info->wifi.config_ap.active = false;
}

void setupWiFiAndKeepAlive(void *pvParameter) {

  // CRITICAL: WiFi will fail to startup without this
  nvs_flash_init();

  WiFi.mode(WIFI_STA); // explicitly set mode, ESP32 defaults to STA+AP

  const char* apName = EBUS_WIFI_CONFIG_AP_NAME;
  // TODO: use generate_ap_password();
  // does not work for some reason, to be investigated
  const char* apPassword = "hollebolle";

  system_info->wifi.rssi = WIFI_NO_SIGNAL;
  system_info->wifi.config_ap.ap_name = apName;
  system_info->wifi.config_ap.ap_password = apPassword;
  system_info->wifi.config_ap.active = true;

  wiFiManager.setConfigPortalBlocking(false);
  wiFiManager.setCaptivePortalEnable(false);
  wiFiManager.setShowInfoUpdate(false);
  wiFiManager.setSaveParamsCallback(saveParamsCallback);
  wiFiManager.setHostname("zebus.home.arpa");

  if (wiFiManager.autoConnect(apName, apPassword)) {
    printf("Connection established!\n");
    system_info->wifi.config_ap.active = false;
  }

  while(1) {
    if (system_info->wifi.config_ap.active) {
      wiFiManager.process();
      vTaskDelay(1);
      continue;
    }
    //wiFiManager.stopConfigPortal();

    if (WiFi.status() == WL_CONNECTED) {
      system_info->wifi.rssi = WiFi.RSSI();
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    printf("WiFi connection was lost\n");
    system_info->wifi.rssi = WIFI_NO_SIGNAL;
    if (WiFi.reconnect()) {
      printf("WiFi connection restored\n");
    }
  }

}
