#ifndef _ESPNOW_TYPE
#define _ESPNOW_TYPE

const uint8_t espnow_broadcast_address[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct master_beacon {
  uint8_t channel;
} master_beacon;

typedef struct outside_temp_message {
  float temperatureC;
} outside_temp_message;

#endif
