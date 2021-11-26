#ifndef ZEBUS_TEMP_CONTROL_H
#define ZEBUS_TEMP_CONTROL_H

#include "zebus-log.h"

// TODO: these are preliminary and should be configurable
#define TEMP_MIN_HEATING 25.0
#define TEMP_OUTSIDE_MIN -7.0
#define TEMP_OUTSIDE_MAX 25.0

#define PID_COMPUTE_INTERVAL_MS (60 * 1000)

#define clamp(in, min, max) (in < min ? min : (in > max ? max : in))

void temparatureControlTask(void *pvParameter);

#endif
