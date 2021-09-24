#include "zebus-messages.h"

#include <stdio.h>
#include <string.h>

#include "zebus-config.h"
#include "zebus-system-info.h"

#define BYTES_TO_WORD(HIGH_BYTE, LOW_BYTE) ((((uint16_t)HIGH_BYTE) << 8) | LOW_BYTE)

#define CMD_IDENTIFICATION 0x0704
#define CMD_DEVICE_CONFIG 0xB509

#define DEVICE_CONFIG_SUBCOMMAND_READ 0x0D

#define DEVICE_CONFIG_FLAME 0x0500
#define DEVICE_CONFIG_HWC_WATERFLOW 0x5500

extern struct system_info_t* system_info;

// prototypes
void debugLogger(Ebus::Telegram telegram);

// Send Response Handlers, these are replies we need to send in Master/Slave communication
// Reply to identification request
uint8_t sendIdentificationResponse(Ebus::Telegram telegram, uint8_t *buffer) {
  uint8_t fixedIdentificationResponse[10] = {0x00};
  fixedIdentificationResponse[0] = EBUS_DEVICE_VENDOR_ID;
  for (uint8_t i = 0; i < 5; i++) {
    fixedIdentificationResponse[i + 1] = strlen(EBUS_DEVICE_NAME) > i ? EBUS_DEVICE_NAME[i] : '-';
  }
  fixedIdentificationResponse[6] = (EBUS_DEVICE_SW_VERSION >> 8) & 0XFF;
  fixedIdentificationResponse[7] = EBUS_DEVICE_SW_VERSION & 0XFF;
  fixedIdentificationResponse[8] = (EBUS_DEVICE_HW_VERSION >> 8) & 0XFF;
  fixedIdentificationResponse[9] = EBUS_DEVICE_HW_VERSION & 0XFF;
  if (BYTES_TO_WORD(telegram.getPB(), telegram.getSB()) == CMD_IDENTIFICATION) {
    memcpy(buffer, fixedIdentificationResponse, sizeof(fixedIdentificationResponse));
    return sizeof(fixedIdentificationResponse);
  }
  return 0;
}

struct message_handler {
    uint16_t command;
    void (*handler)(Ebus::Telegram telegram);
};

void handle_identification(Ebus::Telegram telegram) {
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
    ESP_LOGD(ZEBUS_LOG_TAG, "Self  : %s, sw: %s, hw: %s", identity.device, identity.sw_version, identity.hw_version);
    system_info->ebus.self_id = identity;
    break;
  case EBUS_SLAVE_ADDRESS(EBUS_HEATER_MASTER_ADDRESS):
    ESP_LOGD(ZEBUS_LOG_TAG, "Heater: %s, sw: %s, hw: %s", identity.device, identity.sw_version, identity.hw_version);
    system_info->ebus.heater_id = identity;
    break;
  }
}

void handle_device_config_read_flame(Ebus::Telegram telegram) {
  system_info->ebus.flame = telegram.getResponseByte(0) & 0x0F;
  ESP_LOGD(ZEBUS_LOG_TAG, "Flame : %s", system_info->ebus.flame ? "ON " : "OFF");
}

void handle_device_config_read_hwc_waterflow(Ebus::Telegram telegram) {
  uint16_t flow = BYTES_TO_WORD(telegram.getResponseByte(1), telegram.getResponseByte(0));
  // TODO: FIXME: range of 'flow' is actually litres per minute!
  system_info->ebus.flow = flow; //map(flow, 0, 0xFFFF, 0, 0xFF);
  ESP_LOGD(ZEBUS_LOG_TAG, "Flow  : %f", system_info->ebus.flow / 100.0);
}

message_handler device_config_read_message_handlers[] =
{
    {DEVICE_CONFIG_FLAME, handle_device_config_read_flame},
    {DEVICE_CONFIG_HWC_WATERFLOW, handle_device_config_read_hwc_waterflow},
};


void handle_device_config_read(Ebus::Telegram telegram) {
  if (telegram.getRequestByte(0) == DEVICE_CONFIG_SUBCOMMAND_READ) {
    for (uint8_t i = 0; i < sizeof(device_config_read_message_handlers) / sizeof(message_handler); i++) {
      uint16_t command = BYTES_TO_WORD(telegram.getRequestByte(1), telegram.getRequestByte(2));
      if (command == device_config_read_message_handlers[i].command) {
        device_config_read_message_handlers[i].handler(telegram);
      }
    }
  }
}

message_handler message_handlers[] =
{
    {CMD_IDENTIFICATION, handle_identification},
    {CMD_DEVICE_CONFIG, handle_device_config_read}
};

void handle_error(Ebus::Telegram telegram) {
  ESP_LOGW(ZEBUS_LOG_TAG, "ERROR IN TELEGRAM: %s", telegram.getStateString());
  debugLogger(telegram);
}

void handleMessage(Ebus::Telegram telegram) {
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

void debugLogger(Ebus::Telegram telegram) {
  printf(
    "===========\nstate: %d\nQQ: %02X\tZZ: %02X\tPB: %02X\tSB: %02X\nreq(size: %d, CRC: %02x): ",  //
    telegram.getState(),
    telegram.getQQ(),
    telegram.getZZ(),
    telegram.getPB(),
    telegram.getSB(),
    telegram.getNN(),
    telegram.getRequestCRC());
  for (int i = 0; i < telegram.getNN(); i++) {
     printf(" %02X", telegram.getRequestByte(i));
  }
  printf("\n");
  if (telegram.isResponseExpected()) {
    printf("resp(size: %d, CRC: %02x): ", telegram.getResponseNN(), telegram.getResponseCRC());
    for (int i = 0; i < telegram.getResponseNN(); i++) {
      printf(" %02X", telegram.getResponseByte(i));
    }
    printf("\n");
  }
}
