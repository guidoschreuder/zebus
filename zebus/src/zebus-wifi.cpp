#include "zebus-wifi.h"

#include <Arduino.h>
#include <WiFiManager.h>
#include <esp_now.h>
#include <nvs_flash.h>

#include "espnow-config.h"
#include "espnow-hmac.h"
#include "espnow-types.h"
#include "zebus-config.h"
#include "zebus-events.h"
#include "zebus-system-info.h"
#include "zebus-telegram-bot.h"

const char PWD_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

WiFiManager wiFiManager;
bool wiFiEnabled = false;

// TODO: this name does not cover its meaning anymore
#define BEACON_INTERVAL_MS 5000

// prototype
void setupWiFi();
void runConfigPortal();
void saveConfigPortalParamsCallback();
void onWiFiConnected();
void onWiFiConnectionLost();
void refreshNTP();
void sendEspNowBeacon();
void setupEspNow();
void onEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len);
void onEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

// public functions
const char* generate_ap_password() {
  uint32_t seed = ESP.getEfuseMac() >> 32;
  static char pwd[ZEBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH + 1] = {0};
  for (uint8_t i = 0; i < ZEBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH; i++) {
    pwd[i] = PWD_CHARS[rand_r(&seed) % sizeof(PWD_CHARS)];
  }
  return pwd;
}

void wiFiTask(void *pvParameter) {
  EventGroupHandle_t event_group = (EventGroupHandle_t) pvParameter;
  for(;;) {
    EventBits_t uxBits = xEventGroupGetBits(event_group);
    if (uxBits & WIFI_ENABLED) {
      setupWiFi();

      if (system_info->wifi.config_ap.active) {
        wiFiManager.process();
        vTaskDelay(1);
      } else if (WiFi.status() == WL_CONNECTED) {
        onWiFiConnected();
      } else {
        onWiFiConnectionLost();
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

// implementations
void setupWiFi() {
  if (wiFiEnabled) {
    return;
  }
  // CRITICAL: WiFi will fail to startup without this
  // see: https://github.com/espressif/arduino-esp32/issues/761
  nvs_flash_init();

  WiFi.mode(WIFI_STA); // explicitly set mode, ESP32 defaults to STA+AP
  WiFi.setAutoReconnect(false);

  // disable modem sleep so ESP-NOW packets can be received
  esp_wifi_set_ps(WIFI_PS_NONE);

  setupEspNow();
  runConfigPortal();

  wiFiEnabled = true;
}

void runConfigPortal() {
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

void onWiFiConnected() {
  system_info->wifi.rssi = WiFi.RSSI();
  system_info->wifi.ip_addr = WiFi.localIP();

  refreshNTP();
  handleTelegramMessages();

  vTaskDelay(pdMS_TO_TICKS(BEACON_INTERVAL_MS));
}

void onWiFiConnectionLost() {
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

void refreshNTP() {
  long now = millis();
  if (system_info->ntp.last_init == 0 ||
      system_info->ntp.last_init + ZEBUS_NTP_REFRESH_INTERVAL_SEC * 1000 < now) {
    ESP_LOGD(ZEBUS_LOG_TAG, "Refreshing NTP");
    configTime(ZEBUS_NTP_GMT_OFFSET_SEC, ZEBUS_NTP_GMT_DST_OFFSET_SEC, ZEBUS_NTP_SERVER);
    system_info->ntp.last_init = now;
  }
}

void setupEspNow() {
  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_send_cb(onEspNowDataSent));
  ESP_ERROR_CHECK(esp_now_register_recv_cb(onEspNowDataRecv));
}

void handlePing(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  espnow_msg_ping ping;
  if (!validate_and_copy(&ping, sizeof(ping), incomingData, len)) {
    ESP_LOGE(ZEBUS_LOG_TAG, "%s", getLastHmacError());
    return;
  }
  if (!esp_now_is_peer_exist(mac_addr)) {
    static esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, mac_addr, sizeof(peerInfo.peer_addr));
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));
  }

  espnow_msg_ping_reply ping_reply;
  ping_reply.channel = WiFi.channel();
  ping_reply.expected_interval_millis = ZEBUS_SENSOR_INTERVAL_MS;
  ping_reply.base.type = espnow_ping_reply;

  fillHmac((espnow_msg_base *) &ping_reply, sizeof(ping_reply));

  esp_err_t result = esp_now_send(mac_addr, (uint8_t *)&ping_reply, sizeof(ping_reply));
  if (result != ESP_OK) {
    ESP_LOGE(ZEBUS_LOG_TAG, "There was an error sending ESPNOW ping reply");
  }
}

void handleOutdoorSensor(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  espnow_msg_outdoor_sensor message;
  if (!validate_and_copy(&message, sizeof(message), incomingData, len)) {
    ESP_LOGE(ZEBUS_LOG_TAG, "%s", getLastHmacError());
    return;
  }
  system_info->outdoor.temperatureC = message.temperatureC;
  system_info->outdoor.supplyVoltage = message.supplyVoltage;
  ESP_LOGD(ZEBUS_LOG_TAG, "Outdoor Temperature: %f", system_info->outdoor.temperatureC);
  ESP_LOGD(ZEBUS_LOG_TAG, "Outdoor Voltage: %f", system_info->outdoor.supplyVoltage);
}

void onEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  ESP_LOGV(ZEBUS_LOG_TAG, "Packet received from %02X:%02x:%02x:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  switch (incomingData[0]) {
  case espnow_ping:
    handlePing(mac_addr, incomingData, len);
    break;
  case espnow_outdoor_sensor_data:
    handleOutdoorSensor(mac_addr, incomingData, len);
    break;
  default:
    ESP_LOGW(ZEBUS_LOG_TAG, "Got invalid message type: %d", incomingData[0]);
  }
}

void onEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ESP_LOGV(ZEBUS_LOG_TAG, "Last Packet Send Status: %s", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
