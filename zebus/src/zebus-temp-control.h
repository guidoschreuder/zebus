#ifndef _ZEBUS_TEMP_CONTROL_H
#define _ZEBUS_TEMP_CONTROL_H

#include "zebus-log.h"

// TODO: these are preliminary,
// i.e. TEMP_MAX_HEATING should be read from ebusd message 'FlowSetPotmeter'
// and curve could be set from UI too
#define TEMP_MAX_HEATING 80.0
#define TEMP_MIN_HEATING 25.0
#define TEMP_OUTSIDE_MIN -7.0
#define TEMP_OUTSIDE_MAX 25.0

#define HEATING_CURVE ((TEMP_MAX_HEATING - TEMP_MIN_HEATING) / (TEMP_OUTSIDE_MAX - TEMP_OUTSIDE_MIN))

#define PID_COMPUTE_INTERVAL_MS (60 * 1000)

#define clamp(in, min, max) (in < min ? min : (in > max ? max : in))

void temparatureControlTask(void *pvParameter);

#endif
