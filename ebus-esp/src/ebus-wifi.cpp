#include "ebus-wifi.h"
#include "Arduino.h"

const char PWD_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

const char* generate_ap_password() {
  uint32_t seed = ESP.getEfuseMac() >> 32;
  static char pwd[EBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH + 1] = {0};
  for (uint8_t i = 0; i < EBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH; i++) {
    pwd[i] = PWD_CHARS[rand_r(&seed) % sizeof(PWD_CHARS)];
  }
  return pwd;
}

WiFiManager wiFiManager;

void saveParamsCallback() {
  printf("WiFiManager saveParamsCallback called\n");
  system_info->wifi.config_ap.active = false;
}

void refreshNTP() {
  long now = millis();
  if (system_info->ntp.last_init == 0 ||
      system_info->ntp.last_init + EBUS_NTP_REFRESH_INTERVAL_SEC * 1000 < now) {
    printf("Refreshing NTP\n");
    configTime(EBUS_NTP_GMT_OFFSET_SEC, EBUS_NTP_GMT_DST_OFFSET_SEC, EBUS_NTP_SERVER);
    system_info->ntp.last_init = now;
  }
}

void setupWiFiAndKeepAlive(void *pvParameter) {

  WiFi.mode(WIFI_STA); // explicitly set mode, ESP32 defaults to STA+AP

  char apName[16] = {0};
  sprintf(apName, "%s %x", EBUS_APPNAME, (uint32_t) ESP.getEfuseMac());

  system_info->wifi.rssi = WIFI_NO_SIGNAL;
  system_info->wifi.config_ap.ap_name = strdup(apName);
  system_info->wifi.config_ap.ap_password = generate_ap_password();

  wiFiManager.setConfigPortalBlocking(false);
  wiFiManager.setCaptivePortalEnable(false);
  wiFiManager.setShowInfoUpdate(false);
  wiFiManager.setSaveParamsCallback(saveParamsCallback);
  wiFiManager.setHostname("zebus.home.arpa");

  system_info->wifi.config_ap.active =
      !wiFiManager.autoConnect(system_info->wifi.config_ap.ap_name,
                               system_info->wifi.config_ap.ap_password);
  if (system_info->wifi.config_ap.active) {
    printf("WiFi connection established\n");
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
      system_info->wifi.ip_addr = WiFi.localIP();
      refreshNTP();
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    printf("WiFi connection was lost\n");
    system_info->wifi.rssi = WIFI_NO_SIGNAL;
    if (WiFi.reconnect()) {
      printf("WiFi reconnected\n");
    }
  }

}
