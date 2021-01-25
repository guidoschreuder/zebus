#include "Command.h"
#include "ebus.h"

namespace Ebus {

void Command::pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
  if (waitForEscaped) {
    if (*pos < max_pos) {
      *crc = Ebus::Elf::crc8Calc(cr, *crc);
    }
    buffer[(*pos)] = (cr == 0x0 ? ESC : SYN);
    waitForEscaped = false;
  } else {
    if (*pos < max_pos) {
      *crc = Ebus::Elf::crc8Calc(cr, *crc);
    }
    buffer[(*pos)++] = cr;
    waitForEscaped = (cr == ESC);
  }
}

Command::Command() {
}

Command::Command(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data) {
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

TelegramState Command::getState() {
  return state;
}

void Command::setState(TelegramState newState) {
  state = newState;
}

TelegramType Command::getType() {
  if (getZZ() == ESC) {
    return TelegramType::Unknown;
  }
  if (getZZ() == BROADCAST_ADDRESS) {
    return TelegramType::Broadcast;
  }
  if (Ebus::Elf::isMaster(getZZ())) {
    return TelegramType::MasterMaster;
  }
  return TelegramType::MasterSlave;
}

int16_t Command::getRequestByte(uint8_t pos) {
  if (pos >= getNN()) {
    return -1;
  }
  return requestBuffer[OFFSET_DATA + pos];
}

uint8_t Command::getRequestCRC() {
  return requestBuffer[OFFSET_DATA + getNN()];
}

void Command::pushReqData(uint8_t cr) {
  pushBuffer(cr, requestBuffer, &requestBufferPos, &requestRollingCRC, OFFSET_DATA + getNN());
}

bool Command::isAckExpected() {
  return (getType() != TelegramType::Broadcast);
}

bool Command::isResponseExpected() {
  return (getType() == TelegramType::MasterSlave);
}

bool Command::isRequestComplete() {
  return (state > TelegramState::waitForSyn || state == TelegramState::endCompleted) && (requestBufferPos > OFFSET_DATA) && (requestBufferPos == (OFFSET_DATA + getNN() + 1)) && !waitForEscaped;
}
bool Command::isRequestValid() {
  return isRequestComplete() && getRequestCRC() == requestRollingCRC;
}

bool Command::isFinished() {
  return state < TelegramState::unknown;
}

}
