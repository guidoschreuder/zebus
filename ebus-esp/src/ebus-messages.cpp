#include "system-info.h"
#include "ebus-messages.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

#define BYTES_TO_WORD(HIGH_BYTE, LOW_BYTE) ((((uint16_t) HIGH_BYTE) << 8) | LOW_BYTE)

#define CMD_IDENTIFICATION 0x0704
#define CMD_DEVICE_CONFIG 0xB509

#define DEVICE_CONFIG_SUBCOMMAND_READ 0x0D

#define DEVICE_CONFIG_FLAME 0x0500
#define DEVICE_CONFIG_HWC_WATERFLOW 0x5500

extern struct system_info_t* system_info;

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
    system_info->ebus.self_id = identity;
    break;
  case EBUS_SLAVE_ADDRESS(EBUS_HEATER_MASTER_ADDRESS):
    system_info->ebus.heater_id = identity;
    break;
  }
}

void handle_device_config_read_flame(Ebus::Telegram telegram) {
  system_info->ebus.flame = telegram.getResponseByte(0) & 0x0F;
}

void handle_device_config_read_hwc_waterflow(Ebus::Telegram telegram) {
  uint16_t flow = BYTES_TO_WORD(telegram.getResponseByte(1), telegram.getResponseByte(0));
  // TODO: FIXME: range of 'flow' is actually liters per minute!
  system_info->ebus.flow = flow; //map(flow, 0, 0xFFFF, 0, 0xFF);
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
  printf("ERROR IN TELEGRAM: %d\n", telegram.getState());

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
  printf("Self  : %s, sw: %s, hw: %s\n", system_info->ebus.self_id.device, system_info->ebus.self_id.sw_version, system_info->ebus.self_id.hw_version);
  printf("Heater: %s, sw: %s, hw: %s\n", system_info->ebus.heater_id.device, system_info->ebus.heater_id.sw_version, system_info->ebus.heater_id.hw_version);
  printf("Flame : %s\n", system_info->ebus.flame ? "ON " : "OFF");
  printf("Flow  : %f\n", system_info->ebus.flow / 100.0);

}
