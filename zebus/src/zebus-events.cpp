#include "zebus-events.h"

void eventTask(void *pvParameter) {
  EventGroupHandle_t event_group = (EventGroupHandle_t) pvParameter;
  // TODO: toggling if for demonstration purposes only
  bool displayOn = true, wiFiOn = true;
  for(;;) {
    if (displayOn) {
      xEventGroupSetBits(event_group, DISPLAY_ENABLED);
    } else {
      xEventGroupClearBits(event_group, DISPLAY_ENABLED);
    }
    if (wiFiOn) {
      xEventGroupSetBits(event_group, WIFI_ENABLED);
    } else {
      xEventGroupClearBits(event_group, WIFI_ENABLED);
    }
    displayOn = !displayOn;
    wiFiOn = !wiFiOn;
    vTaskDelay(pdMS_TO_TICKS(20000));
  }
}
