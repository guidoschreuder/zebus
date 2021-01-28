#ifndef __EBUS_SEND_COMMAND
#define __EBUS_SEND_COMMAND

#include <stdint.h>

#include "TelegramBase.h"

namespace Ebus {

class SendCommand : public TelegramBase {
  SendCommandState state = SendCommandState::waitForSend;
  uint8_t numTries = 0;

  public:
  SendCommand();
  SendCommand(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data);
  SendCommandState getState();
  void setState(SendCommandState new_state);
  bool canRetry(int8_t max_tries);
};

}  // namespace Ebus

#endif
