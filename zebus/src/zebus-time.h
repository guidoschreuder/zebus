#ifndef _ZEBUS_UTIL_H
#define _ZEBUS_UTIL_H

#include "soc/rtc.h"
extern "C" {
  #include <esp32/clk.h>
}

#define get_rtc_millis() (rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get()) / 1000)

bool interval_expired(uint64_t *last, uint64_t interval_ms);

#endif
