#ifndef EBUS_DEFINES_H
#define EBUS_DEFINES_H

#define EBUS_SLAVE_ADDRESS(MASTER) ((uint8_t) (((MASTER) + 5) % 0xFF))

#define EBUS_SYN 0xAA
#define EBUS_ESC 0xA9
#define EBUS_ACK 0x00
#define EBUS_NACK 0xFF

#define EBUS_BROADCAST_ADDRESS 0xFE

#endif
