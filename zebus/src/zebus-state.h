#ifndef ZEBUS_STATE_H
#define ZEBUS_STATE_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include "zebus-log.h"

enum state_bits {
  DISPLAY_ENABLED  = (1 << 0),
  DISPLAY_DISABLED = (1 << 1),
  WIFI_ENABLED     = (1 << 2),
  WIFI_DISABLED    = (1 << 3),
};

void stateTask(void *pvParameter);

#endif
