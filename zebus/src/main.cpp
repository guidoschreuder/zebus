#include "main.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sdkconfig.h"
#include "zebus-display.h"
#include "zebus-ebus.h"
#include "zebus-system-info.h"
#include "zebus-wifi.h"

struct system_info_t* system_info = new system_info_t();

extern "C" {
void app_main();
}

void app_main() {
  ESP_LOGI(ZEBUS_LOG_TAG, "Setup %s", ZEBUS_APPNAME);

  xTaskCreate(&displayTask, "displayTask", 2048, NULL, 5, NULL);
  xTaskCreate(&wiFiTask, "wiFiTask", 8192, NULL, 3, NULL);
  xTaskCreate(&ebusTask, "ebusTask", 2048, NULL, 5, NULL);
}
