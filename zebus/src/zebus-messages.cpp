#include "zebus-messages.h"

#include <stdio.h>
#include <string.h>

#include "zebus-config.h"
#include "zebus-system-info.h"

#define VERIFY_RESPONSE_LENGTH(min_length) \
  if (telegram.getResponseNN() < min_length) { \
    ESP_LOGE(ZEBUS_LOG_TAG, "RESPONSE TOO SHORT: is %d, expected at least %d bytes", telegram.getResponseNN(), min_length); \
    debugLogger(telegram); \
    return; \
  }

#define BYTES_TO_WORD(HIGH_BYTE, LOW_BYTE) ((((uint16_t)HIGH_BYTE) << 8) | LOW_BYTE)
#define GET_BYTE(CMD, I) ((uint8_t) ((CMD >> 8 * I) & 0XFF))

extern struct system_info_t* system_info;

struct message_handler {
    uint16_t command;
    void (*handler)(Ebus::Telegram &telegram);
};

// prototypes
void handle_identification(Ebus::Telegram &telegram);
void handle_device_config_read(Ebus::Telegram &telegram);
void handle_device_config_read_flame(Ebus::Telegram &telegram);
void handle_device_config_read_hwc_waterflow(Ebus::Telegram &telegram);
void handle_device_config_read_flow_temp(Ebus::Telegram &telegram);
void handle_device_config_read_return_temp(Ebus::Telegram &telegram);
void handle_device_config_read_ebus_control(Ebus::Telegram &telegram);
void handle_device_config_read_partload_hc_kw(Ebus::Telegram &telegram);
void handle_device_config_read_modulation(Ebus::Telegram &telegram);
void handle_device_config_read_max_flow_setpoint(Ebus::Telegram &telegram);
void handle_error(Ebus::Telegram &telegram);
void debugLogger(Ebus::Telegram &telegram);

message_handler message_handlers[] =
{
    {CMD_IDENTIFICATION, handle_identification},
    {CMD_DEVICE_CONFIG, handle_device_config_read}
};

message_handler device_config_read_message_handlers[] =
{
    {DEVICE_CONFIG_FLAME, handle_device_config_read_flame},
    {DEVICE_CONFIG_HWC_WATERFLOW, handle_device_config_read_hwc_waterflow},
    {DEVICE_CONFIG_FLOW_TEMP, handle_device_config_read_flow_temp},
    {DEVICE_CONFIG_RETURN_TEMP, handle_device_config_read_return_temp},
    {DEVICE_CONFIG_EBUS_CONTROL, handle_device_config_read_ebus_control},
    {DEVICE_CONFIG_PARTLOAD_HC_KW, handle_device_config_read_partload_hc_kw},
    {DEVICE_CONFIG_MODULATION, handle_device_config_read_modulation},
    {DEVICE_CONFIG_MAX_FLOW_SETPOINT, handle_device_config_read_max_flow_setpoint},
};

// public functions
void handleMessage(Ebus::Telegram &telegram) {
  if (telegram.getState() != Ebus::TelegramState::endCompleted) {
      handle_error(telegram);
      return;
  }
  for (uint8_t i = 0; i < sizeof(message_handlers) / sizeof(message_handler); i++) {
    uint16_t command = BYTES_TO_WORD(telegram.getPB(), telegram.getSB());
    if (command == message_handlers[i].command) {
      message_handlers[i].handler(telegram);
    }
  }
}

// Send Response Handlers, these are replies we need to send in Master/Slave communication.
/**
 * Reply to identification request
*/
uint8_t sendIdentificationResponse(Ebus::Telegram &telegram, uint8_t *buffer) {
  if (BYTES_TO_WORD(telegram.getPB(), telegram.getSB()) != CMD_IDENTIFICATION) {
    return 0;
  }
  uint8_t fixedIdentificationResponse[10] = {0x00};
  fixedIdentificationResponse[0] = EBUS_DEVICE_VENDOR_ID;
  for (uint8_t i = 0; i < 5; i++) {
    fixedIdentificationResponse[i + 1] = strlen(EBUS_DEVICE_NAME) > i ? EBUS_DEVICE_NAME[i] : '-';
  }
  fixedIdentificationResponse[6] = (EBUS_DEVICE_SW_VERSION >> 8) & 0XFF;
  fixedIdentificationResponse[7] = EBUS_DEVICE_SW_VERSION & 0XFF;
  fixedIdentificationResponse[8] = (EBUS_DEVICE_HW_VERSION >> 8) & 0XFF;
  fixedIdentificationResponse[9] = EBUS_DEVICE_HW_VERSION & 0XFF;

  memcpy(buffer, fixedIdentificationResponse, sizeof(fixedIdentificationResponse));
  return sizeof(fixedIdentificationResponse);
}

Ebus::SendCommand createCommand(uint8_t target, unsigned short command) {
  return createCommand(  //
      target,
      command,
      0,
      NULL);
}

Ebus::SendCommand createCommand(uint8_t target, unsigned short command, uint8_t NN, uint8_t *data) {
  return Ebus::SendCommand(  //
      EBUS_MASTER_ADDRESS,
      Ebus::Ebus::Elf::isMaster(target) ? EBUS_SLAVE_ADDRESS(target) : target,
      GET_BYTE(command, 1),
      GET_BYTE(command, 0),
      NN,
      data);
}

Ebus::SendCommand createHeaterCommand(unsigned short command) {
  return createHeaterCommand( //
      command,
      0,
      NULL);
}

Ebus::SendCommand createHeaterCommand(unsigned short command, uint8_t NN, uint8_t *data) {
  return createCommand(  //
      EBUS_HEATER_MASTER_ADDRESS,
      command,
      NN,
      data);
}

Ebus::SendCommand createHeaterReadConfigCommand(unsigned short config_element) {
  uint8_t data[] = { DEVICE_CONFIG_SUBCOMMAND_READ, GET_BYTE(config_element, 1), GET_BYTE(config_element, 0)};
  return createHeaterCommand(CMD_DEVICE_CONFIG, sizeof(data), data);
}

// implementations
void handle_identification(Ebus::Telegram &telegram) {
  VERIFY_RESPONSE_LENGTH(10);
  identification_t identity = identification_t {
      .device = {0},
      .sw_version = {0},
      .hw_version = {0},
  };
  for (int i = 0; i < 5; i++) {
    identity.device[i] = telegram.getResponseByte(i + 1);
  }
  sprintf(identity.sw_version, "%02X.%02X", (uint8_t)telegram.getResponseByte(6), (uint8_t)telegram.getResponseByte(7));
  sprintf(identity.hw_version, "%02X.%02X", (uint8_t)telegram.getResponseByte(8), (uint8_t)telegram.getResponseByte(9));

  switch (telegram.getZZ()) {
  case EBUS_SLAVE_ADDRESS(EBUS_MASTER_ADDRESS):
    ESP_LOGD(ZEBUS_LOG_TAG, "Self: %s, sw: %s, hw: %s", identity.device, identity.sw_version, identity.hw_version);
    system_info->ebus.self_id = identity;
    break;
  case EBUS_SLAVE_ADDRESS(EBUS_HEATER_MASTER_ADDRESS):
    ESP_LOGD(ZEBUS_LOG_TAG, "Heater: %s, sw: %s, hw: %s", identity.device, identity.sw_version, identity.hw_version);
    system_info->ebus.heater_id = identity;
    break;
  }
}

void handle_device_config_read(Ebus::Telegram &telegram) {
  if (telegram.getRequestByte(0) == DEVICE_CONFIG_SUBCOMMAND_READ) {
    for (uint8_t i = 0; i < sizeof(device_config_read_message_handlers) / sizeof(message_handler); i++) {
      uint16_t command = BYTES_TO_WORD(telegram.getRequestByte(1), telegram.getRequestByte(2));
      if (command == device_config_read_message_handlers[i].command) {
        device_config_read_message_handlers[i].handler(telegram);
      }
    }
  }
}

void handle_device_config_read_flame(Ebus::Telegram &telegram) {
  VERIFY_RESPONSE_LENGTH(1);
  system_info->heater.flame = telegram.getResponseByte(0) & 0x0F;
  ESP_LOGD(ZEBUS_LOG_TAG, "Flame: %s", system_info->heater.flame ? "ON " : "OFF");
}

void handle_device_config_read_hwc_waterflow(Ebus::Telegram &telegram) {
  VERIFY_RESPONSE_LENGTH(2);
  system_info->heater.flow = BYTES_TO_WORD(telegram.getResponseByte(1), telegram.getResponseByte(0)) / 100.0;
  ESP_LOGD(ZEBUS_LOG_TAG, "Flow: %.2f", system_info->heater.flow);
}

void handle_device_config_read_flow_temp(Ebus::Telegram &telegram) {
  VERIFY_RESPONSE_LENGTH(2);
  system_info->heater.flow_temp = BYTES_TO_WORD(telegram.getResponseByte(1), telegram.getResponseByte(0)) / 16.0;
  ESP_LOGD(ZEBUS_LOG_TAG, "Flow Temp: %.2f", system_info->heater.flow_temp);
}

void handle_device_config_read_return_temp(Ebus::Telegram &telegram) {
  VERIFY_RESPONSE_LENGTH(2);
  system_info->heater.return_temp = BYTES_TO_WORD(telegram.getResponseByte(1), telegram.getResponseByte(0)) / 16.0;
  ESP_LOGD(ZEBUS_LOG_TAG, "Return Temp: %.2f", system_info->heater.return_temp);
}

void handle_device_config_read_ebus_control(Ebus::Telegram &telegram) {
  VERIFY_RESPONSE_LENGTH(1);
  ESP_LOGD(ZEBUS_LOG_TAG, "EBus Heat Control: %s", telegram.getResponseByte(0) & 0x0F ? "YES" : "NO");
}

void handle_device_config_read_partload_hc_kw(Ebus::Telegram &telegram) {
  VERIFY_RESPONSE_LENGTH(1);
  ESP_LOGD(ZEBUS_LOG_TAG, "Partload HC KW: %d", telegram.getResponseByte(0));
}

void handle_device_config_read_modulation(Ebus::Telegram &telegram) {
  VERIFY_RESPONSE_LENGTH(2);
  system_info->heater.modulation = BYTES_TO_WORD(telegram.getResponseByte(1), telegram.getResponseByte(0)) / 10.0;
  ESP_LOGD(ZEBUS_LOG_TAG, "Modulation: %.1f", system_info->heater.modulation);
}

void handle_device_config_read_max_flow_setpoint(Ebus::Telegram &telegram) {
  VERIFY_RESPONSE_LENGTH(2);
  system_info->heater.max_flow_setpoint = BYTES_TO_WORD(telegram.getResponseByte(1), telegram.getResponseByte(0)) / 16.0;
  ESP_LOGD(ZEBUS_LOG_TAG, "Max Flow Setpoint: %.1f", system_info->heater.max_flow_setpoint);
}

void handle_error(Ebus::Telegram &telegram) {
  ESP_LOGW(ZEBUS_LOG_TAG, "ERROR IN TELEGRAM: %s", telegram.getStateString());
  debugLogger(telegram);
}

void debugLogger(Ebus::Telegram &telegram) {
  printf(
    "===========\nstate: %d\nQQ: %02X\tZZ: %02X\tPB: %02X\tSB: %02X\nreq(size: %d, CRC: %02x): ",  //
    telegram.getState(),
    telegram.getQQ(),
    telegram.getZZ(),
    telegram.getPB(),
    telegram.getSB(),
    telegram.getNN(),
    telegram.getRequestCRC());
  for (int8_t i = 0; i < telegram.getNN(); i++) {
    int16_t reqByte = telegram.getRequestByte(i);
    if (reqByte == -1) {
      printf(" -> REQUEST OVERFLOW");
      break;
    }
    printf(" %02X", (uint8_t) reqByte);
  }
  printf("\n");
  if (telegram.isResponseExpected()) {
    printf("resp(size: %d, CRC: %02x): ", telegram.getResponseNN(), telegram.getResponseCRC());
    for (int i = 0; i < telegram.getResponseNN(); i++) {
      int16_t respByte = telegram.getResponseByte(i);
      if (respByte == -1) {
        printf(" -> RESPONSE OVERFLOW");
        break;
      }
      printf(" %02X", (uint8_t) respByte);
    }
    printf("\n");
  }
}
