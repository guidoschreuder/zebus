#include "Arduino.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#include "WiFi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "espnow-config.h"
#include "esp_now.h"

#define ONE_WIRE_PIN 4
#define TIME_TO_SLEEP_SECONDS 10

#define uS_TO_S(seconds) (seconds * 1000000)

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

// MAC: AC:67:B2:36:AA:DC
uint8_t broadcastAddress[] = {0xAC, 0x67, 0xB2, 0x36, 0xAA, 0xDC};

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {

  delay(100);

  WiFi.mode(WIFI_STA);

  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
  ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));

  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_send_cb(OnDataSent));

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, sizeof(broadcastAddress));
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

  sensors.begin();
}

void loop() {

  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  printf("Temperature: %f\n", temperatureC);

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &temperatureC, sizeof(temperatureC));

  if (result == ESP_OK) {
    printf("The message was sent successfully.\n");
  } else {
    printf("There was an error sending the message.\n");
  }

  esp_sleep_enable_timer_wakeup(uS_TO_S(TIME_TO_SLEEP_SECONDS)); // ESP32 wakes after timer expires
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

  printf("Uptime: %ld\n", millis());

  esp_deep_sleep_start();
}
