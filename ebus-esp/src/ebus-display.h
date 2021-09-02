#ifndef _EBUS_DISPLAY
#define _EBUS_DISPLAY

#include <TFT_eSPI.h>
#include "esb-flame.h"

// Display variables
extern TFT_eSPI tft;

void setupDisplay();

void updateDisplay(void *pvParameter);

#endif
