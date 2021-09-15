#include "ebus-wifi.h"
#include "espnow-types.h"
#include "espnow-config.h"

#include "Arduino.h"
#include "esp_log.h"
#include "esp_now.h"

const char PWD_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

bool espNowInit = false;

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

void OnEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  printf("Packet received from %02X:%02x:%02x:%02X:%02X:%02X\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  outside_temp_message data;
  memcpy(&data, incomingData, sizeof(data));
  printf("Temperature: %f\n", data.temperatureC);
}

void OnEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setupEspNow() {
  if (!espNowInit) {
    ESP_ERROR_CHECK(esp_now_init());

    ESP_ERROR_CHECK(esp_now_register_send_cb(OnEspNowDataSent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(OnEspNowDataRecv));

    static esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, espnow_broadcast_address, sizeof(espnow_broadcast_address));
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    espNowInit = true;
  }
}

void sendEspNowBeacon() {
  setupEspNow();

  master_beacon beacon;
  beacon.channel = WiFi.channel();

  esp_err_t result = esp_now_send(espnow_broadcast_address, (uint8_t *)&beacon, sizeof(beacon));

  if (result == ESP_OK) {
    printf("Beacon was sent successfully.\n");
  } else {
    printf("There was an error sending the beacon.\n");
  }
}

void wiFiLoop(void *pvParameter) {

  WiFi.mode(WIFI_STA); // explicitly set mode, ESP32 defaults to STA+AP

  // disable modem sleep so ESP-NOW packets can be received
  esp_wifi_set_ps(WIFI_PS_NONE);

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

  printf("Channel: %d, MAC: %s\n", WiFi.channel(), WiFi.macAddress().c_str());

  while(1) {
    if (system_info->wifi.config_ap.active) {
      wiFiManager.process();
      vTaskDelay(1);
      continue;
    }
    //wiFiManager.stopConfigPortal();
    sendEspNowBeacon();

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
