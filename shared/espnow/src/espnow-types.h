#ifndef ESPNOW_TYPES_H
#define ESPNOW_TYPES_H

const uint8_t espnow_broadcast_address[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

enum espnow_message_type {
  espnow_ping,
  espnow_ping_reply,
  espnow_temperature_sensor_data,
};

typedef struct espnow_msg_base {
  uint8_t type;
  uint8_t payloadHmac[32];
  uint64_t nonce;
} espnow_message_base;

typedef struct espnow_msg_ping {
  espnow_message_base base;
} espnow_msg_ping;

typedef struct espnow_msg_ping_reply {
  espnow_message_base base;
  uint8_t channel;
  uint64_t expected_interval_millis;
} espnow_msg_ping_reply;

typedef struct espnow_msg_temperature_sensor {
  espnow_message_base base;
  char location[16];
  float temperatureC;
  float supplyVoltage;
} espnow_msg_temperature_sensor;

#endif
