#include "zebus-wifi.h"

#include <WiFiManager.h>
#include <nvs_flash.h>

#include "zebus-config.h"
#include "zebus-events.h"
#include "zebus-state.h"
#include "zebus-system-info.h"
#include "zebus-telegram-bot.h"
#include "zebus-time.h"
#include "zebus-secrets.h"
#include "zebus-espnow.h"
#include "zebus-mqtt.h"

const char PWD_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

WiFiManager wiFiManager;
bool wiFiEnabled = false;
RTC_DATA_ATTR bool configPortalHasRan = false;

uint64_t ntp_last_init;

// prototype
void setupWiFi();
void disableWiFi();
void runConfigPortal();
void saveConfigPortalParamsCallback();
void onWiFiConnected();
void onWiFiConnectionLost();
void refreshNTP();

// public functions
const char *generate_ap_password() {
  uint32_t seed = ESP.getEfuseMac() >> 32;
  static char pwd[ZEBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH + 1] = {0};
  for (uint8_t i = 0; i < ZEBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH; i++) {
    pwd[i] = PWD_CHARS[rand_r(&seed) % sizeof(PWD_CHARS)];
  }
  return pwd;
}

void wiFiTask(void *pvParameter) {
  // CRITICAL: WiFi will fail to startup without this
  // see: https://github.com/espressif/arduino-esp32/issues/761
  nvs_flash_init();

  EventGroupHandle_t event_group = (EventGroupHandle_t)pvParameter;
  for (;;) {
    // always wait for the config portal to finish before turning off WiFi
    if (system_info->wifi.config_ap.active) {
      wiFiManager.process();
      vTaskDelay(1);
      continue;
    }

    EventBits_t uxBits = xEventGroupGetBits(event_group);
    if (uxBits & WIFI_ENABLED) {
      setupWiFi();

      if (WiFi.status() == WL_CONNECTED) {
        onWiFiConnected();
      } else {
        onWiFiConnectionLost();
      }
    } else if (wiFiEnabled) {
      disableWiFi();
      xEventGroupSetBits(event_group, WIFI_DISABLED);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// implementations
void setupWiFi() {
  if (wiFiEnabled) {
    return;
  }
  ESP_LOGI(ZEBUS_LOG_TAG, "Setup WiFi");

  esp_wifi_set_storage(WIFI_STORAGE_FLASH);

  // explicitly set mode, ESP32 defaults to STA+AP
  esp_wifi_set_mode(WIFI_MODE_STA);
  WiFi.setAutoReconnect(false);

  // disable modem sleep so ESP-NOW packets can be received
  // TODO: this is not very power efficient, investigate if we can estimate when ESP_NOW packets are expected to be received
  esp_wifi_set_ps(WIFI_PS_NONE);

  if (!configPortalHasRan) {
    runConfigPortal();
    configPortalHasRan = true;
  } else {
    esp_wifi_start();
  }

  espnow_setup();
  mqtt_setup();

  system_info->wifi.rssi = WIFI_NO_SIGNAL;

  wiFiEnabled = true;
}

void disableWiFi() {
  ESP_LOGI(ZEBUS_LOG_TAG, "Disable WiFi");

  system_info->wifi.config_ap.active = false;

  espnow_disable();

  // set storage to RAM so saved settings are not erased
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_disconnect();
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_stop();

  wiFiEnabled = false;
}

void runConfigPortal() {
  char apName[16] = {0};
  snprintf(apName, sizeof(apName), "%s %x", ZEBUS_APPNAME, (uint32_t)ESP.getEfuseMac());

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
}

void onWiFiConnectionLost() {
  ESP_LOGW(ZEBUS_LOG_TAG, "WiFi connection was lost");
  system_info->wifi.rssi = WIFI_NO_SIGNAL;

  esp_wifi_set_storage(WIFI_STORAGE_FLASH);
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
  if (interval_expired(&ntp_last_init, ZEBUS_NTP_REFRESH_INTERVAL_MS)) {
    ESP_LOGD(ZEBUS_LOG_TAG, "Refreshing NTP");
    configTime(ZEBUS_NTP_GMT_OFFSET_SEC, ZEBUS_NTP_GMT_DST_OFFSET_SEC, ZEBUS_NTP_SERVER);
  }
}
