#include "Arduino.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_PIN 4
#define TIME_TO_SLEEP_SECONDS 10

#define uS_TO_S(seconds) (seconds * 1000000)

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);

void setup() {
  sensors.begin();
}

void loop() {

  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  printf("Temperature: %f\n", temperatureC);

  esp_sleep_enable_timer_wakeup(uS_TO_S(TIME_TO_SLEEP_SECONDS)); // ESP32 wakes after timer expires
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

  esp_deep_sleep_start();
}
