#ifndef __EBUS_DEFINES
#define __EBUS_DEFINES

// for testing in native unit test we do not have ESP32 so fake it here
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#define EBUS_SLAVE_ADDRESS(MASTER) (((MASTER) + 5) % 0xFF)

#define EBUS_MAX_TRIES 2

#define SYN 0xAA
#define ESC 0xA9
#define ACK 0x00
#define NACK 0xFF

#define BROADCAST_ADDRESS 0xFE

#endif

