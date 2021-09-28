#ifndef _ZEBUS_MESSAGES_H
#define _ZEBUS_MESSAGES_H

#include "Ebus.h"
#include "zebus-log.h"

#define CMD_IDENTIFICATION 0x0704
#define CMD_DEVICE_CONFIG 0xB509

#define DEVICE_CONFIG_SUBCOMMAND_READ 0x0D

#define DEVICE_CONFIG_FLAME 0x0500
#define DEVICE_CONFIG_HWC_WATERFLOW 0x5500
#define DEVICE_CONFIG_HWC_DEMAND 0x5800

struct identification_t {
   char device[6];
   char sw_version[6];
   char hw_version[6];
};

void handleMessage(Ebus::Telegram &telegram);

uint8_t sendIdentificationResponse(Ebus::Telegram &telegram, uint8_t *buffer);

Ebus::SendCommand createCommand(uint8_t target, unsigned short command);
Ebus::SendCommand createCommand(uint8_t target, unsigned short command, uint8_t NN, uint8_t *data);
Ebus::SendCommand createHeaterCommand(unsigned short command);
Ebus::SendCommand createHeaterCommand(unsigned short command, uint8_t NN, uint8_t *data);
Ebus::SendCommand createHeaterReadConfigCommand(unsigned short config_element);

#endif
