#ifndef _EBUS_TELEGRAM
#define _EBUS_TELEGRAM

#include "Command.h"

namespace Ebus {
class Telegram : public Command {
  uint8_t responseBuffer[RESPONSE_BUFFER_SIZE] = {0};
  uint8_t responseBufferPos = 0;
  uint8_t responseRollingCRC = 0;

  public:
  Telegram();

  Telegram(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data);

  uint8_t getResponseNN() {
    return responseBuffer[0];
  }

  int16_t getResponseByte(uint8_t pos);
  uint8_t getResponseCRC();

  void pushRespData(uint8_t cr);
  bool isResponseComplete();
  bool isResponseValid();
};

}  // namespace Ebus

#endif
