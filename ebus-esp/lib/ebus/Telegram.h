#ifndef _EBUS_TELEGRAM
#define _EBUS_TELEGRAM

#include "TelegramBase.h"

namespace Ebus {
class Telegram : public TelegramBase {
  TelegramState state = TelegramState::waitForSyn;
  uint8_t responseBuffer[RESPONSE_BUFFER_SIZE] = {0};
  uint8_t responseBufferPos = 0;
  uint8_t responseRollingCRC = 0;

  public:
  Telegram();
  TelegramState getState();
  void setState(TelegramState newState);

  uint8_t getResponseNN() {
    return responseBuffer[0];
  }

  int16_t getResponseByte(uint8_t pos);
  uint8_t getResponseCRC();

  void pushRespData(uint8_t cr);
  bool isResponseComplete();
  bool isResponseValid();
  bool isRequestComplete();
  bool isRequestValid();
  bool isFinished();
};

}  // namespace Ebus

#endif
