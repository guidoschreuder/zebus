#include "network/zebus-espnow.h"

#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "espnow-config.h"
#include "espnow-hmac.h"
#include "espnow-types.h"
#include "zebus-datatypes.h"
#include "zebus-events.h"
#include "zebus-log.h"
#include "zebus-time.h"

// prototypes
void espnow_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);
void espnow_data_received(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void handle_espnow_ping(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void handle_espnow_temperature_sensor(const uint8_t *mac_addr, const uint8_t *incomingData, int len);

// implementation
void espnow_setup() {
  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_data_sent));
  ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_data_received));
}

void espnow_disable() {
  esp_now_deinit();
}

void espnow_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ESP_LOGV(ZEBUS_LOG_TAG, "Last Packet Send Status: %s", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void espnow_data_received(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  ESP_LOGV(ZEBUS_LOG_TAG, "Packet received from %02X:%02x:%02x:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  switch (incomingData[0]) {
  case espnow_ping:
    handle_espnow_ping(mac_addr, incomingData, len);
    break;
  case espnow_temperature_sensor_data:
    handle_espnow_temperature_sensor(mac_addr, incomingData, len);
    break;
  default:
    ESP_LOGW(ZEBUS_LOG_TAG, "Got invalid message type: %d", incomingData[0]);
  }
}

void handle_espnow_ping(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
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

  wifi_second_chan_t secondCh;
  esp_wifi_get_channel(&ping_reply.channel, &secondCh);

  ping_reply.expected_interval_millis = ZEBUS_SENSOR_INTERVAL_MS;
  ping_reply.base.type = espnow_ping_reply;

  fillHmac((espnow_msg_base *)&ping_reply, sizeof(ping_reply));

  esp_err_t result = esp_now_send(mac_addr, (uint8_t *)&ping_reply, sizeof(ping_reply));
  if (result != ESP_OK) {
    ESP_LOGE(ZEBUS_LOG_TAG, "There was an error sending ESPNOW ping reply");
  }
}

void handle_espnow_temperature_sensor(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  espnow_msg_temperature_sensor message;
  if (!validate_and_copy(&message, sizeof(message), incomingData, len)) {
    ESP_LOGE(ZEBUS_LOG_TAG, "%s", getLastHmacError());
    return;
  }

  measurement_temperature_sensor data;

  data.timestamp = get_rtc_millis();
  data.value = message;

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
