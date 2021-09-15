#ifndef _ZEBUS_DISPLAY
#define _ZEBUS_DISPLAY

#include <TFT_eSPI.h>

// Display variables
extern TFT_eSPI tft;

void setupDisplay();

void updateDisplay(void *pvParameter);

#endif
