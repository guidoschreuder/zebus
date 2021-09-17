#include "zebus-wifi.h"

#include <Arduino.h>
#include <WiFiManager.h>
#include <esp_now.h>

#include "espnow-config.h"
#include "espnow-types.h"
#include "zebus-config.h"
#include "zebus-system-info.h"

const char PWD_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

WiFiManager wiFiManager;

bool espNowInit = false;

#define BEACON_INTERVAL_MS 5000
uint32_t lastBeacon = 0;

// prototype
void setupWiFi();
void startConfigPortal();
void saveConfigPortalParamsCallback();
void refreshNTP();
void sendEspNowBeacon();
void setupEspNow();
void OnEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len);
void OnEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

// public functions
const char* generate_ap_password() {
  uint32_t seed = ESP.getEfuseMac() >> 32;
  static char pwd[ZEBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH + 1] = {0};
  for (uint8_t i = 0; i < ZEBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH; i++) {
    pwd[i] = PWD_CHARS[rand_r(&seed) % sizeof(PWD_CHARS)];
  }
  return pwd;
}

void wiFiLoop(void *pvParameter) {

  setupWiFi();
  startConfigPortal();

  while(1) {
    if (system_info->wifi.config_ap.active) {
      wiFiManager.process();
      vTaskDelay(1);
      continue;
    }
    //wiFiManager.stopConfigPortal();

    ESP_LOGD(ZEBUS_LOG_TAG, "WiFi status: %d", WiFi.status());
    if (WiFi.status() == WL_CONNECTED) {
      system_info->wifi.rssi = WiFi.RSSI();
      system_info->wifi.ip_addr = WiFi.localIP();
      refreshNTP();
      sendEspNowBeacon();

      vTaskDelay(pdMS_TO_TICKS(BEACON_INTERVAL_MS));
      continue;
    }
    ESP_LOGW(ZEBUS_LOG_TAG, "WiFi connection was lost");
    system_info->wifi.rssi = WIFI_NO_SIGNAL;

    ESP_ERROR_CHECK(esp_wifi_connect());
    for (int i = 0; i < 20 && (WiFi.status() != WL_CONNECTED); i++) {
      vTaskDelay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
      ESP_LOGI(ZEBUS_LOG_TAG, "WiFi reconnection SUCCESS");
    } else {
      ESP_LOGW(ZEBUS_LOG_TAG, "WiFi reconnection FAILED");
    }
  }
}

// implementations
void setupWiFi() {
  WiFi.mode(WIFI_STA); // explicitly set mode, ESP32 defaults to STA+AP
  WiFi.setAutoReconnect(false);

  // disable modem sleep so ESP-NOW packets can be received
  esp_wifi_set_ps(WIFI_PS_NONE);
}

void startConfigPortal() {
  char apName[16] = {0};
  sprintf(apName, "%s %x", ZEBUS_APPNAME, (uint32_t) ESP.getEfuseMac());

  system_info->wifi.rssi = WIFI_NO_SIGNAL;
  system_info->wifi.config_ap.ap_name = strdup(apName);
  system_info->wifi.config_ap.ap_password = generate_ap_password();

  wiFiManager.setConfigPortalBlocking(false);
  wiFiManager.setCaptivePortalEnable(false);
  wiFiManager.setShowInfoUpdate(false);
  wiFiManager.setSaveParamsCallback(saveConfigPortalParamsCallback);
  wiFiManager.setHostname(ZEBUS_WIFI_HOSTNAME);

  system_info->wifi.config_ap.active =
      !wiFiManager.autoConnect(system_info->wifi.config_ap.ap_name,
                               system_info->wifi.config_ap.ap_password);

  if (system_info->wifi.config_ap.active) {
    ESP_LOGI(ZEBUS_LOG_TAG, "WiFi Configuration Portal is activated");
  }
}

void saveConfigPortalParamsCallback() {
  ESP_LOGI(ZEBUS_LOG_TAG, "WiFiManager saveParamsCallback called");
  system_info->wifi.config_ap.active = false;
}

void refreshNTP() {
  long now = millis();
  if (system_info->ntp.last_init == 0 ||
      system_info->ntp.last_init + ZEBUS_NTP_REFRESH_INTERVAL_SEC * 1000 < now) {
    ESP_LOGD(ZEBUS_LOG_TAG, "Refreshing NTP");
    configTime(ZEBUS_NTP_GMT_OFFSET_SEC, ZEBUS_NTP_GMT_DST_OFFSET_SEC, ZEBUS_NTP_SERVER);
    system_info->ntp.last_init = now;
  }
}

void sendEspNowBeacon() {
  if (lastBeacon + BEACON_INTERVAL_MS > millis()) {
    return;
  }

  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  ESP_LOGD(ZEBUS_LOG_TAG, "Sending ESP-NOW beacon");

  setupEspNow();

  wifi_country_t country;
  ESP_ERROR_CHECK(esp_wifi_get_country(&country));

  master_beacon beacon;
  beacon.channel = WiFi.channel();

  esp_wifi_disconnect();

  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
  for (uint8_t channel = country.schan; channel < country.schan + country.nchan; channel++) {
    esp_err_t res;
    while ((res = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE)) != ESP_OK) {
      ESP_LOGE(ZEBUS_LOG_TAG, "Failed to set WiFi channel for sending beacon: %s", esp_err_to_name(res));
      esp_wifi_disconnect();
    }

    esp_err_t result = esp_now_send(espnow_broadcast_address, (uint8_t *)&beacon, sizeof(beacon));
    if (result != ESP_OK) {
      ESP_LOGE(ZEBUS_LOG_TAG, "There was an error sending the beacon on channel %d.\n", channel);
    }
  }
  ESP_ERROR_CHECK(esp_wifi_set_channel(beacon.channel, WIFI_SECOND_CHAN_NONE));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));

  lastBeacon = millis();

  ESP_ERROR_CHECK(esp_wifi_connect());
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

void OnEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  ESP_LOGD(ZEBUS_LOG_TAG, "Packet received from %02X:%02x:%02x:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  outside_temp_message data;
  memcpy(&data, incomingData, sizeof(data));
  ESP_LOGD(ZEBUS_LOG_TAG, "Temperature: %f", data.temperatureC);
}

void OnEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ESP_LOGV(ZEBUS_LOG_TAG, "Last Packet Send Status: %s", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
