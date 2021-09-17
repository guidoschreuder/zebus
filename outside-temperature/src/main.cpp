#include <DallasTemperature.h>
#include <OneWire.h>

#include "Arduino.h"
#include "WiFi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "espnow-config.h"
#include "espnow-types.h"
#include <driver/adc.h>

#define ONE_WIRE_PIN 4
#define TIME_TO_SLEEP_SECONDS 10
#define MAX_SEND_ATTEMPT 3

#define ADC_SAMPLES 8

#define uS_TO_S(seconds) (seconds * 1000000)

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

RTC_DATA_ATTR bool master_beacon_received = false;
RTC_DATA_ATTR master_beacon beacon;
RTC_DATA_ATTR uint8_t master_mac_addr[6] = {0};

uint8_t sendAttempt = 0;
bool sendGotResult = false;
bool sendSuccess = false;

void OnEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&beacon, incomingData, sizeof(beacon));
  memcpy(&master_mac_addr, mac_addr, sizeof(master_mac_addr));
  master_beacon_received = true;
}

void OnEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  sendGotResult = true;
  sendSuccess = (status == ESP_NOW_SEND_SUCCESS);

  if (!sendSuccess) {
    printf("Packet failed to deliver at attempt %d of %d\n", sendAttempt, MAX_SEND_ATTEMPT);
  }
}

esp_err_t doSendData(outside_temp_message data) {
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

  return esp_now_send(master_mac_addr, (uint8_t *) &data, sizeof(data));
}

void sendData(outside_temp_message data) {
  while(!master_beacon_received) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  do {
    sendAttempt++;
    sendGotResult = false;

    if (sendAttempt > 1) {
      vTaskDelay(pdMS_TO_TICKS(200));
    }
    esp_err_t result = doSendData(data);
    if (result != ESP_OK) {
      printf("Failed to send packet with reason '%s' at attempt %d of %d\n", esp_err_to_name(result), sendAttempt, MAX_SEND_ATTEMPT);
    } else {
      while(!sendGotResult) {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
  } while(!sendSuccess && sendAttempt < MAX_SEND_ATTEMPT);

  printf("Send result %s after %d attempt(s)\n", sendSuccess ? "OK" : "FAIL", sendAttempt);

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

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_11db);

  int val = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    val += adc1_get_raw(ADC1_CHANNEL_4);
  }
  printf("ADC: %f\n", (val * 3.8 / 3700) / ADC_SAMPLES);

  sendData(data);

  esp_sleep_enable_timer_wakeup(uS_TO_S(TIME_TO_SLEEP_SECONDS)); // ESP32 wakes after timer expires
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

  printf("Uptime: %ld\n", millis());

  esp_deep_sleep_start();
}
