#include "zebus-mqtt.h"

#include <mqtt_client.h>
#include <WString.h>

#include "zebus-config.h"
#include "zebus-events.h"
#include "zebus-secrets.h"
#include "zebus-datatypes.h"

esp_mqtt_client_handle_t mqtt_client;
uint64_t last_mqtt_sent = 0;

// declarations
void mqtt_handle_float(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void mqtt_handle_bool(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void mqtt_handle_sensor(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

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

