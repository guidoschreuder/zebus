#include "main.h"

#include "zebus-display.h"
#include "zebus-ebus.h"
#include "zebus-events.h"
#include "zebus-system-info.h"
#include "zebus-wifi.h"

struct system_info_t* system_info = new system_info_t();

static EventGroupHandle_t zebus_event_group;

extern "C" {
void app_main();
}

void app_main() {
  ESP_LOGI(ZEBUS_LOG_TAG, "Setup %s", ZEBUS_APPNAME);

  zebus_event_group = xEventGroupCreate();

  xTaskCreate(&displayTask, "displayTask", 2048, zebus_event_group, 5, NULL);
  xTaskCreate(&wiFiTask, "wiFiTask", 8192, zebus_event_group, 3, NULL);
  xTaskCreate(&ebusTask, "ebusTask", 2048, NULL, 5, NULL);
  xTaskCreate(&eventTask, "eventTask", 2048, zebus_event_group, 5, NULL);
}
