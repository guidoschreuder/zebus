#include <DallasTemperature.h>
#include <OneWire.h>
#include <driver/adc.h>

#include "Arduino.h"
#include "WiFi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "espnow-config.h"
#include "espnow-hmac.h"
#include "espnow-types.h"

#define ONE_WIRE_PIN 4
#define TIME_TO_SLEEP_SECONDS 10
#define MAX_SEND_ATTEMPT 3

#define ADC_SAMPLES 16
#define ADC_REF_VOLTAGE 3.281
#define ADC_REF_SAMPLE 3701

#define uS_TO_S(seconds) (seconds * 1000000)

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

#define INVALID_CHANNEL -1
RTC_DATA_ATTR bool master_found = false;
RTC_DATA_ATTR int8_t master_channel = INVALID_CHANNEL;
RTC_DATA_ATTR uint8_t master_mac_addr[6] = {0};

uint8_t sendAttempt = 0;
bool sendGotResult = false;
bool sendSuccess = false;

// declarations
void initEspNow();
void onEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len);
void onEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
bool findMaster();
esp_err_t doSendData(espnow_msg_outdoor_sensor data);
void sendData(espnow_msg_outdoor_sensor data);
float getOutsideTemp();
float getSupplyVoltage();

// implementations, public
void setup() {
  WiFi.mode(WIFI_STA);

  // setup ESP-NOW
  initEspNow();

  // setup tenperature sensor
  sensors.begin();

  // setup ADC
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_11db);
}

void loop() {
  if (findMaster()) {
    espnow_msg_outdoor_sensor data;
    data.base.type = espnow_outdoor_sensor_data;
    data.temperatureC = getOutsideTemp();
    data.supplyVoltage = getSupplyVoltage();
    sendData(data);
  } else {
    printf("Master not found\n");
  }

  esp_sleep_enable_timer_wakeup(uS_TO_S(TIME_TO_SLEEP_SECONDS));
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

  printf("Uptime: %ld\n", millis());
  printf("Enter deep sleep\n");
  esp_deep_sleep_start();
}

// implementations, local
void initEspNow() {
  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_send_cb(onEspNowDataSent));
  ESP_ERROR_CHECK(esp_now_register_recv_cb(onEspNowDataRecv));

  static esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, espnow_broadcast_address, sizeof(espnow_broadcast_address));
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));
}

void onEspNowDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  if (incomingData[0] == espnow_ping_reply) {
    espnow_msg_ping_reply ping_reply;
    if (len != sizeof(ping_reply)) {
      printf("Invalid packet length, expected %d, got %d\n", sizeof(ping_reply), len);
      return;
    }

    memcpy(&ping_reply, incomingData, sizeof(ping_reply));
    if (!verifyHmac((espnow_msg_base *) &ping_reply, sizeof(ping_reply))) {
      printf("Invalid HMAC\n");
      return;
    }

    master_channel = ping_reply.channel;
    memcpy(&master_mac_addr, mac_addr, sizeof(master_mac_addr));
    master_found = true;
    printf("Master found at channel: %d\n", master_channel);
 }
}

void onEspNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  sendGotResult = true;
  sendSuccess = (status == ESP_NOW_SEND_SUCCESS);

  if (!sendSuccess) {
    printf("Packet failed to deliver at attempt %d of %d\n", sendAttempt, MAX_SEND_ATTEMPT);
  }
}

bool findMaster() {
  if (master_found) {
    return true;
  }

  wifi_country_t country;
  ESP_ERROR_CHECK(esp_wifi_get_country(&country));

  espnow_msg_ping ping;
  ping.base.type = espnow_ping;
  fillHmac((espnow_msg_base *) &ping, sizeof(ping));

  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
  for (uint8_t channel = country.schan; channel < country.schan + country.nchan  && !master_found; channel++) {
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

    esp_err_t result = esp_now_send(espnow_broadcast_address, (uint8_t *)&ping, sizeof(ping));
    if (result != ESP_OK) {
      printf("There was an error sending ping on channel %d\n", channel);
      continue;
    }
    // wait for response
    for (int i = 0; !master_found && i < 10; i++) {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));

  return master_found;
}

esp_err_t doSendData(espnow_msg_outdoor_sensor data) {
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
  ESP_ERROR_CHECK(esp_wifi_set_channel(master_channel, WIFI_SECOND_CHAN_NONE));
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));

  fillHmac((espnow_msg_base *) &data, sizeof(data));

  return esp_now_send(master_mac_addr, (uint8_t *) &data, sizeof(data));
}

void sendData(espnow_msg_outdoor_sensor data) {
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

float getOutsideTemp() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);
  printf("Temperature: %f\n", temp);
  return temp;
}

float getSupplyVoltage() {
  int val = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    val += adc1_get_raw(ADC1_CHANNEL_4);
  }
  float result = (val * ADC_REF_VOLTAGE) / (ADC_REF_SAMPLE * ADC_SAMPLES);
  printf("Voltage: sample: %d => %f\n", val / ADC_SAMPLES, result);
  return result;
}
