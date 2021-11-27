#include "zebus-temp-control.h"

#include "PID.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "zebus-datatypes.h"
#include "zebus-events.h"

bool tempControlInit = false;

measurement_temperature_sensor current_room_temp;
measurement_temperature_sensor outdoor_temp;
measurement_float max_flow_setpoint;

double input_room_temp;
double desired_room_temp;
double heat_request_temp;

double Kp = 2, Ki = 5, Kd = 1;

PID pid = PID((double*) &input_room_temp, &heat_request_temp, &desired_room_temp, Kp, Ki, Kd, DIRECT);

// prototypes
void initTempControl();
float get_max_weather_flow_temp(float outside_temp, float flow_setpoint);

// public functions
void temparatureControlTask(void *pvParameter) {
  while (1) {
    initTempControl();

    if (outdoor_temp.valid() &&
        current_room_temp.valid() &&
        max_flow_setpoint.valid()) {

      // TODO: should be set by user on room controller
      desired_room_temp = 19.5;

      input_room_temp = current_room_temp.value.temperatureC;

      pid.SetOutputLimits(TEMP_MIN_HEATING,
                          get_max_weather_flow_temp(outdoor_temp.value.temperatureC,
                                                    max_flow_setpoint.value));

      pid.Compute();

      ESP_LOGD(ZEBUS_LOG_TAG, "PID heating target: %.2f", heat_request_temp);
    } else {
      ESP_LOGW(ZEBUS_LOG_TAG, "no valid data for PID loop");
    }

    vTaskDelay(pdMS_TO_TICKS(PID_COMPUTE_INTERVAL_MS));
  }
}

void temp_control_handle_max_flow_setpoint(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  max_flow_setpoint = *((measurement_float*)event_data);
}

void temp_control_handle_sensor(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  measurement_temperature_sensor data = *((measurement_temperature_sensor*) event_data);
  if (strcmp(data.value.location, "room") == 0) {
    current_room_temp = data;
  } else if (strcmp(data.value.location, "outdoor") == 0) {
    outdoor_temp = data;
  }
}

// implementations
void initTempControl() {
  if (tempControlInit) {
    return;
  }
  pid.SetMode(AUTOMATIC);
  tempControlInit = true;

  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_MAX_FLOW_SETPOINT, temp_control_handle_max_flow_setpoint, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_SENSOR, temp_control_handle_sensor, NULL));
}

float get_max_weather_flow_temp(float outside_temp, float flow_setpoint) {
  float curve = (flow_setpoint - TEMP_MIN_HEATING) / (TEMP_OUTSIDE_MAX - TEMP_OUTSIDE_MIN);
  return clamp(flow_setpoint - (outside_temp - TEMP_OUTSIDE_MIN) * curve, TEMP_MIN_HEATING, flow_setpoint);
}
