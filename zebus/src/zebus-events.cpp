#include "zebus-events.h"

ESP_EVENT_DEFINE_BASE(ZEBUS_EVENTS);

void initEventLoop() {
  esp_event_loop_create_default();
}
