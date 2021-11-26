#ifndef EBUS_TELEGRAM_H
#define EBUS_TELEGRAM_H

#include "TelegramBase.h"

namespace Ebus {
class Telegram : public TelegramBase {
  uint8_t responseBuffer[EBUS_RESPONSE_BUFFER_SIZE] = {0};
  uint8_t responseBufferPos = 0;
  uint8_t responseRollingCRC = 0;

  public:
  Telegram();

  uint8_t getResponseNN() {
    uint8_t nn = responseBuffer[0];
    if (nn >= EBUS_MAX_DATA_LENGTH) {
      return 0;
    }
    return nn;
  }

  int16_t getResponseByte(uint8_t pos);
  uint8_t getResponseCRC();

  void pushRespData(uint8_t cr);
  bool isResponseComplete();
  bool isResponseValid();
  bool isRequestComplete();
  bool isRequestValid();
};

}  // namespace Ebus

#endif
