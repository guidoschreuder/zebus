#include "TelegramBase.h"

#include "Ebus.h"

namespace Ebus {

TelegramBase::TelegramBase() {
}

TelegramState TelegramBase::getState() {
  return state;
}

void TelegramBase::setState(TelegramState newState) {
  state = newState;
}


void TelegramBase::pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
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

TelegramType TelegramBase::getType() {
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

int16_t TelegramBase::getRequestByte(uint8_t pos) {
  if (pos > getNN()) {
    return -1;
  }
  return requestBuffer[OFFSET_DATA + pos];
}

uint8_t TelegramBase::getRequestCRC() {
  return requestBuffer[OFFSET_DATA + getNN()];
}

void TelegramBase::pushReqData(uint8_t cr) {
  pushBuffer(cr, requestBuffer, &requestBufferPos, &requestRollingCRC, OFFSET_DATA + getNN());
}

bool TelegramBase::isAckExpected() {
  return (getType() != TelegramType::Broadcast);
}

bool TelegramBase::isResponseExpected() {
  return (getType() == TelegramType::MasterSlave);
}

bool TelegramBase::isFinished() {
  return state < TelegramState::unknown;
}

}  // namespace Ebus
