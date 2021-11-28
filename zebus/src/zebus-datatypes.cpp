#include "zebus-datatypes.h"


bool measurement_valid(measurement& measurement) {
  return measurement.timestamp != 0;
}
