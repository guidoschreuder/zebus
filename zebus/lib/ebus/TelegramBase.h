#ifndef EBUS_COMMAND_H
#define EBUS_COMMAND_H

#include "ebus-defines.h"
#include "ebus-enums.h"

/* Specification says:
   1. In master and slave telegram part, standardised commands must be limited to 10 used data bytes.
   2. In master and slave telegram part, the sum of mfr.-specific telegram used data bytes must not exceed 14.
   We use 16 to be on the safe side for now.
*/
#define EBUS_MAX_DATA_LENGTH 16

#define EBUS_OFFSET_QQ 0
#define EBUS_OFFSET_ZZ 1
#define EBUS_OFFSET_PB 2
#define EBUS_OFFSET_SB 3
#define EBUS_OFFSET_NN 4
#define EBUS_OFFSET_DATA 5
#define EBUS_REQUEST_BUFFER_SIZE (EBUS_OFFSET_DATA + EBUS_MAX_DATA_LENGTH + 1)
#define EBUS_RESPONSE_BUFFER_SIZE (EBUS_MAX_DATA_LENGTH + 2)
#define EBUS_RESPONSE_OFFSET 1

#define EBUS_INVALID_RESPONSE_BYTE -1

#define _EBUS_GETTER(BUFFER, POS)     \
  uint8_t get##POS() {                \
    return BUFFER[EBUS_OFFSET_##POS]; \
  }

namespace Ebus {


class TelegramBase {
  protected:
  TelegramState state;
  uint8_t requestBuffer[EBUS_REQUEST_BUFFER_SIZE] = {EBUS_ESC, EBUS_ESC};  // initialize QQ and ZZ with ESC char to distinguish from valid master 0
  uint8_t requestBufferPos = 0;
  uint8_t requestRollingCRC = 0;
  bool waitForEscaped = false;
  void pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos);

  public:
  TelegramBase();

  _EBUS_GETTER(requestBuffer, QQ);
  _EBUS_GETTER(requestBuffer, ZZ);
  _EBUS_GETTER(requestBuffer, PB);
  _EBUS_GETTER(requestBuffer, SB);
  uint8_t getNN();

  TelegramState getState();
  const char * getStateString();

  void setState(TelegramState newState);
  TelegramType getType();
  int16_t getRequestByte(uint8_t pos);
  uint8_t getRequestCRC();
  void pushReqData(uint8_t cr);
  bool isAckExpected();
  bool isResponseExpected();
  bool isFinished();

};

}

#endif
