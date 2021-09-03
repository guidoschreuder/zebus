#include "ebus-messages.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

#define COMMAND(PB, SB) ((((uint16_t) PB) << 8) | SB)

#define CMD_IDENTIFICATION 0x0704

extern struct info_t* system_info;

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
    system_info->self = identity;
    break;
  case EBUS_SLAVE_ADDRESS(EBUS_HEATER_MASTER_ADDRESS):
    system_info->heater = identity;
    break;
  }
}

struct message_handler {
    uint16_t command;
    void (*handler)(Ebus::Telegram telegram);
};

message_handler message_handlers[] =
{
    {CMD_IDENTIFICATION, handle_identification},
};

void handle_error(Ebus::Telegram telegram) {

}

void handleMessage(Ebus::Telegram telegram) {
  if (telegram.getState() != Ebus::TelegramState::endCompleted) {
      handle_error(telegram);
  }
  for (uint8_t i = 0; i < sizeof(message_handlers) / sizeof(message_handler); i++) {
    uint16_t command = COMMAND(telegram.getPB(), telegram.getSB());
    if (command == message_handlers[i].command) {
      message_handlers[i].handler(telegram);
    }
  }
}
