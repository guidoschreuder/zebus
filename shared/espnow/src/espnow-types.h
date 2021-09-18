#ifndef _ESPNOW_TYPE
#define _ESPNOW_TYPE

const uint8_t espnow_broadcast_address[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct master_beacon_message {
  uint8_t channel;
} master_beacon_message;

typedef struct outdoor_sensor_message {
  float temperatureC;
  float supplyVoltage;
} outdoor_sensor_message;

#endif
