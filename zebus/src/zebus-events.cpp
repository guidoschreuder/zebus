#include "zebus-events.h"

void eventTask(void *pvParameter) {
  EventGroupHandle_t event_group = (EventGroupHandle_t) pvParameter;
  bool displayOn = true;
  for(;;) {
    // TODO: display toggling if for demonstration purpose only
    if (displayOn) {
      xEventGroupSetBits(event_group, DISPLAY_ENABLED);
    } else {
      xEventGroupClearBits(event_group, DISPLAY_ENABLED);
    }
    displayOn = !displayOn;
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
