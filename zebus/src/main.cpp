#include "main.h"

#include "display/zebus-display.h"
#include "network/zebus-wifi.h"
#include "zebus-ebus.h"
#include "zebus-events.h"
#include "zebus-state.h"
#include "zebus-system-info.h"
#include "zebus-temp-control.h"

struct system_info_t* system_info = new system_info_t();

static EventGroupHandle_t zebus_state_event_group;

extern "C" {
void app_main();
}

void app_main() {
  ESP_LOGI(ZEBUS_LOG_TAG, "Setup %s", ZEBUS_APPNAME);

  zebus_state_event_group = xEventGroupCreate();

  initEventLoop();

  xTaskCreate(&displayTask, "displayTask", 2048, zebus_state_event_group, 5, NULL);
  xTaskCreate(&wiFiTask, "wiFiTask", 8192, zebus_state_event_group, 3, NULL);
  xTaskCreate(&ebusTask, "ebusTask", 2048, NULL, 5, NULL);
  xTaskCreate(&stateTask, "stateTask", 2048, zebus_state_event_group, 5, NULL);
  xTaskCreate(&temparatureControlTask, "temparatureControlTask", 2048, NULL, 4, NULL);
}
