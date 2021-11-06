#include "zebus-time.h"

bool interval_expired(uint64_t *last, uint64_t interval_ms) {
  uint64_t now = get_rtc_millis();
  if (*last == 0 ||                 // first time
      *last + interval_ms < now ||  // interval has passed
      *last > now) {                // system time has been adjusted backwards
    *last = now;
    return true;
  }
  return false;
}
