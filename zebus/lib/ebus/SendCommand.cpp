#include "SendCommand.h"

namespace Ebus {

SendCommand::SendCommand() {
  state = TelegramState::endCompleted;
}

SendCommand::SendCommand(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data) {
  state = TelegramState::waitForSend;
  pushReqData(QQ);
  pushReqData(ZZ);
  pushReqData(PB);
  pushReqData(SB);
  pushReqData(NN);
  for (int i = 0; i < NN; i++) {
    pushReqData(data[i]);
  }
  pushReqData(requestRollingCRC);
}

bool SendCommand::canRetry(int8_t max_tries) {
  return numTries++ < max_tries;
}

uint8_t SendCommand::getCRC() {
  return requestRollingCRC;
}

}  // namespace Ebus