#ifndef _EBUS_COMMAND
#define _EBUS_COMMAND

#include "ebus-defines.h"
#include "ebus-enums.h"

#define MAX_DATA_LENGTH 16

#define OFFSET_QQ 0
#define OFFSET_ZZ 1
#define OFFSET_PB 2
#define OFFSET_SB 3
#define OFFSET_NN 4
#define OFFSET_DATA 5
#define REQUEST_BUFFER_SIZE (OFFSET_DATA + MAX_DATA_LENGTH + 1)
#define RESPONSE_BUFFER_SIZE (MAX_DATA_LENGTH + 2)
#define RESPONSE_OFFSET 1

#define _GETTER(BUFFER, POS)     \
  uint8_t get##POS() {           \
    return BUFFER[OFFSET_##POS]; \
  }

namespace Ebus {


class Command {
  TelegramState state = TelegramState::waitForSyn;
  uint8_t requestBuffer[REQUEST_BUFFER_SIZE] = {ESC, ESC};  // initialize QQ and ZZ with ESC char to distinguish from valid master 0
  uint8_t requestBufferPos = 0;
  uint8_t requestRollingCRC = 0;

  protected:
  bool waitForEscaped = false;
  void pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos);

  public:
  Command();
  Command(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data);

  _GETTER(requestBuffer, QQ);
  _GETTER(requestBuffer, ZZ);
  _GETTER(requestBuffer, PB);
  _GETTER(requestBuffer, SB);
  _GETTER(requestBuffer, NN);

  TelegramState getState();
  void setState(TelegramState newState);
  TelegramType getType();
  int16_t getRequestByte(uint8_t pos);
  uint8_t getRequestCRC();
  void pushReqData(uint8_t cr);
  bool isAckExpected();
  bool isResponseExpected();
  bool isRequestComplete();
  bool isRequestValid();
  bool isFinished();
};

}

#endif
