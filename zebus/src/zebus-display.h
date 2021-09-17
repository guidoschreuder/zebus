#ifndef _ZEBUS_DISPLAY
#define _ZEBUS_DISPLAY

#include "zebus-log.h"
#include "zebus-config.h"

#include <TFT_eSPI.h>

void setupDisplay();

void updateDisplay(void *pvParameter);

#endif
