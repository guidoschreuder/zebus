#include "SendCommand.h"

namespace Ebus {

SendCommand::SendCommand() {
  state = SendCommandState::endSendCompleted;
}

SendCommand::SendCommand(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data) {
  state = SendCommandState::waitForSend;
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

SendCommandState SendCommand::getState() {
  return state;
}
void SendCommand::setState(SendCommandState new_state) {
  state = new_state;
}

bool SendCommand::canRetry(int8_t max_tries) {
  return numTries++ < max_tries;
}

}  // namespace Ebus