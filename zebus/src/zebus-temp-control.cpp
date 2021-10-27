#include "zebus-temp-control.h"

#include "PID.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zebus-system-info.h"

bool tempControlInit = false;

double current_room_temp;
double desired_room_temp;
double heat_request_temp;

double Kp = 2, Ki = 5, Kd = 1;

PID pid = PID(&current_room_temp, &heat_request_temp, &desired_room_temp, Kp, Ki, Kd, DIRECT);

// prototypes
void initTempControl();
float get_max_weather_flow_temp(float outside_temp);

// public functions
void temparatureControlTask(void *pvParameter) {
  while (1) {
    initTempControl();

    // TODO: should be read by room controller
    current_room_temp = 16.0;
    // TODO: should be set by user on room controller
    desired_room_temp = 19.5;

    pid.SetOutputLimits(TEMP_MIN_HEATING, get_max_weather_flow_temp(system_info->outdoor.temperatureC));

    pid.Compute();

    ESP_LOGD(ZEBUS_LOG_TAG, "PID heating target: %.2f", heat_request_temp);

    vTaskDelay(pdMS_TO_TICKS(PID_COMPUTE_INTERVAL_MS));
  }
}

// implementations
void initTempControl() {
  if(tempControlInit) {
      return;
  }
  pid.SetOutputLimits(TEMP_MIN_HEATING, TEMP_MAX_HEATING);
  pid.SetMode(AUTOMATIC);
  tempControlInit = true;
}


float get_max_weather_flow_temp(float outside_temp) {
  return clamp(TEMP_MAX_HEATING - (outside_temp - TEMP_OUTSIDE_MIN) * HEATING_CURVE, TEMP_MIN_HEATING, TEMP_MAX_HEATING);
}
