#include "zebus-wifi.h"

#include <WiFiManager.h>
#include <esp_now.h>
#include <mqtt_client.h>
#include <nvs_flash.h>

#include "espnow-config.h"
#include "espnow-hmac.h"
#include "espnow-types.h"
#include "zebus-config.h"
#include "zebus-events.h"
#include "zebus-state.h"
#include "zebus-system-info.h"
#include "zebus-telegram-bot.h"
#include "zebus-time.h"
#include "zebus-secrets.h"

const char PWD_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

WiFiManager wiFiManager;
bool wiFiEnabled = false;
RTC_DATA_ATTR bool configPortalHasRan = false;

esp_mqtt_client_handle_t mqtt_client;
uint64_t last_mqtt_sent = 0;

uint64_t ntp_last_init;

// prototype
void setupWiFi();
void disableWiFi();
void runConfigPortal();
void saveConfigPortalParamsCallback();
void onWiFiConnected();
void onWiFiConnectionLost();
void refreshNTP();
void sendEspNowBeacon();
void setupEspNow();
void onEspNowDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void onEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void mqtt_setup();

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

  setupEspNow();
  mqtt_setup();

  system_info->wifi.rssi = WIFI_NO_SIGNAL;

  wiFiEnabled = true;
}

void disableWiFi() {
  ESP_LOGI(ZEBUS_LOG_TAG, "Disable WiFi");

  system_info->wifi.config_ap.active = false;

  esp_now_deinit();

  // set storage to RAM so saved settings are not erased
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_disconnect();
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_stop();

  wiFiEnabled = false;
}

void runConfigPortal() {
  char apName[16] = {0};
  sprintf(apName, "%s %x", ZEBUS_APPNAME, (uint32_t)ESP.getEfuseMac());

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

  fillHmac((espnow_msg_base *)&ping_reply, sizeof(ping_reply));

  esp_err_t result = esp_now_send(mac_addr, (uint8_t *)&ping_reply, sizeof(ping_reply));
  if (result != ESP_OK) {
    ESP_LOGE(ZEBUS_LOG_TAG, "There was an error sending ESPNOW ping reply");
  }
}

void handleTemperatureSensor(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  espnow_msg_temperature_sensor message;
  if (!validate_and_copy(&message, sizeof(message), incomingData, len)) {
    ESP_LOGE(ZEBUS_LOG_TAG, "%s", getLastHmacError());
    return;
  }

  measurement_temperature_sensor data;

  data.timestamp = get_rtc_millis();
  memcpy(&data.value, &message, sizeof(message));

  ESP_LOGD(ZEBUS_LOG_TAG, "Temperature Sensor: {location: %s, temp: %f, voltage: %f}",
           message.location,
           message.temperatureC,
           message.supplyVoltage);

  esp_event_post(ZEBUS_EVENTS,
                 zebus_events::EVNT_RECVD_SENSOR,
                 &data,
                 sizeof(measurement_temperature_sensor),
                 portMAX_DELAY);
}

void onEspNowDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  ESP_LOGV(ZEBUS_LOG_TAG, "Packet received from %02X:%02x:%02x:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  switch (incomingData[0]) {
  case espnow_ping:
    handlePing(mac_addr, incomingData, len);
    break;
  case espnow_temperature_sensor_data:
    handleTemperatureSensor(mac_addr, incomingData, len);
    break;
  default:
    ESP_LOGW(ZEBUS_LOG_TAG, "Got invalid message type: %d", incomingData[0]);
  }
}

void onEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ESP_LOGV(ZEBUS_LOG_TAG, "Last Packet Send Status: %s", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

#define MQTT_LOG(predicate, client, topic, value) do { \
  if (predicate) { \
    esp_mqtt_client_publish(client, topic, String(value).c_str(), 0, 1, 0); \
  } \
} while(0)

void mqtt_handle_float(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  measurement_float data = *((measurement_float*) event_data);
  MQTT_LOG(true, mqtt_client, ((const char*) event_handler_arg), data.value);
}

void mqtt_handle_bool(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  measurement_bool data = *((measurement_bool*) event_data);
  MQTT_LOG(true, mqtt_client, ((const char*) event_handler_arg), data.value);
}

void mqtt_handle_sensor(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  measurement_temperature_sensor data = *((measurement_temperature_sensor*) event_data);
  String topic_base = "zebus/sensor/" + String(data.value.location);
  MQTT_LOG(true, mqtt_client, (topic_base + "/temperature").c_str(), data.value.temperatureC);
  MQTT_LOG(true, mqtt_client, (topic_base + "/voltage").c_str(), data.value.supplyVoltage);
}

void mqtt_setup() {
  esp_mqtt_client_config_t mqtt_cfg = {
      .host = ZEBUS_MQTT_HOST,
      .username = ZEBUS_MQTT_USERNAME,
      .password = ZEBUS_MQTT_PASSWORD,
      .transport = ZEBUS_MQTT_TRANSPORT,
  };
  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
  // esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  esp_mqtt_client_start(mqtt_client);

  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_SENSOR, mqtt_handle_sensor, NULL));

  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_MAX_FLOW_SETPOINT, mqtt_handle_float, (void*) "zebus/heater/setpoint/flow/temperature"));
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_FLOW, mqtt_handle_float, (void*) "zebus/heater/hcw/flowrate"));
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_FLOW_TEMP, mqtt_handle_float, (void*) "zebus/heater/flow/temperature"));
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_RETURN_TEMP, mqtt_handle_float, (void*) "zebus/heater/return/temperature"));
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_MODULATION, mqtt_handle_float, (void*) "zebus/heater/modulation/percentage"));

  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_FLAME, mqtt_handle_bool, (void*) "zebus/heater/flame/onoff"));
}
