#include <DallasTemperature.h>
#include <OneWire.h>

#include "Arduino.h"
#include "WiFi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "espnow-config.h"
#include "espnow-types.h"

#define ONE_WIRE_PIN 4
#define TIME_TO_SLEEP_SECONDS 10

#define uS_TO_S(seconds) (seconds * 1000000)

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

RTC_DATA_ATTR bool master_beacon_received = false;
RTC_DATA_ATTR master_beacon beacon;
RTC_DATA_ATTR uint8_t master_mac_addr[6] = {0};

void OnEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&beacon, incomingData, sizeof(beacon));
  memcpy(&master_mac_addr, mac_addr, sizeof(master_mac_addr));
  master_beacon_received = true;
}

void OnEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    printf("Packet failed to deliver\n");
  }
}

void sendData(outside_temp_message data) {
  if (!master_beacon_received) {
    printf("no beacon detected yet\n");
    return;
  }
  if (!esp_now_is_peer_exist(master_mac_addr)) {

    // NOTE: needs to be declared `static` otherwise the follow error occurs:
    //   E (3263) ESPNOW: Peer interface is invalid
    //   ESP_ERROR_CHECK failed: esp_err_t 0x3066 (ESP_ERR_ESPNOW_ARG) at 0x40088970
    static esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, master_mac_addr, sizeof(peerInfo.peer_addr));
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));
  }

  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
  ESP_ERROR_CHECK(esp_wifi_set_channel(beacon.channel, WIFI_SECOND_CHAN_NONE));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));

  ESP_ERROR_CHECK(esp_now_send(master_mac_addr, (uint8_t *) &data, sizeof(data)));
}

void setup() {

  WiFi.mode(WIFI_STA);

  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_send_cb(OnEspNowDataSent));
  ESP_ERROR_CHECK(esp_now_register_recv_cb(OnEspNowDataRecv));

  sensors.begin();
}

void loop() {

  sensors.requestTemperatures();

  outside_temp_message data;
  data.temperatureC = sensors.getTempCByIndex(0);

  printf("Temperature: %f\n", data.temperatureC);
  while(!master_beacon_received) {
    vTaskDelay(10);
  }
  sendData(data);

  esp_sleep_enable_timer_wakeup(uS_TO_S(TIME_TO_SLEEP_SECONDS)); // ESP32 wakes after timer expires
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

  printf("Uptime: %ld\n", millis());

  esp_deep_sleep_start();
}
