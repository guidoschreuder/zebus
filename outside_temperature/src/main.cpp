#include "Arduino.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#include "WiFi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "espnow-types.h"
#include "espnow-config.h"
#include "esp_now.h"

#define ONE_WIRE_PIN 4
#define TIME_TO_SLEEP_SECONDS 10

#define uS_TO_S(seconds) (seconds * 1000000)

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

RTC_DATA_ATTR bool master_beacon_received = false;
RTC_DATA_ATTR master_beacon beacon;
RTC_DATA_ATTR uint8_t master_mac_addr[6] = {0};

void OnEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  printf("Packet received from %02X:%02x:%02x:%02X:%02X:%02X\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

  memcpy(&beacon, incomingData, sizeof(beacon));
  printf("Master lives at channel: %d\n", beacon.channel);

  memcpy(&master_mac_addr, mac_addr, sizeof(master_mac_addr));
  master_beacon_received = true;
}

void OnEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
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

  esp_err_t result = esp_now_send(master_mac_addr, (uint8_t *) &data, sizeof(data));

  if (result == ESP_OK) {
    printf("The message was sent successfully.\n");
  } else {
    printf("There was an error sending the message.\n");
  }

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
