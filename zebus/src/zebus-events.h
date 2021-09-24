#ifndef _ZEBUS_EVENTS_H
#define _ZEBUS_EVENTS_H

#include "freertos/FreeRTOS.h"
#include <freertos/event_groups.h>

#include "zebus-log.h"

enum event_bits {
  DISPLAY_ENABLED  = (1 << 0),
  DISPLAY_SHUTDOWN = (1 << 1),
};

void eventTask(void *pvParameter);

#endif
