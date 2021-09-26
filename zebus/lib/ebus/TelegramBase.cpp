#include "TelegramBase.h"

#include "Ebus.h"

namespace Ebus {

TelegramBase::TelegramBase() {
}

uint8_t TelegramBase::getNN() {
  uint8_t nn = requestBuffer[EBUS_OFFSET_NN];
  if (nn >= EBUS_MAX_DATA_LENGTH) {
    return 0;
  }
  return nn;
}

TelegramState TelegramBase::getState() {
  return state;
}

#define X(name, int) case int: return ""#name"";
const char * TelegramBase::getStateString() {
  switch((int8_t) state) {
    TELEGRAM_STATE_TABLE
    default:
      return "[INVALID STATE]";
  }
}
#undef X

void TelegramBase::setState(TelegramState newState) {
  state = newState;
}


void TelegramBase::pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
  if (waitForEscaped) {
    if (*pos < max_pos) {
      *crc = Ebus::Elf::crc8Calc(cr, *crc);
    }
    buffer[(*pos)] = (cr == 0x0 ? EBUS_ESC : EBUS_SYN);
    waitForEscaped = false;
  } else {
    if (*pos < max_pos) {
      *crc = Ebus::Elf::crc8Calc(cr, *crc);
    }
    buffer[(*pos)++] = cr;
    waitForEscaped = (cr == EBUS_ESC);
  }
}

TelegramType TelegramBase::getType() {
  if (getZZ() == EBUS_ESC) {
    return TelegramType::Unknown;
  }
  if (getZZ() == EBUS_BROADCAST_ADDRESS) {
    return TelegramType::Broadcast;
  }
  if (Ebus::Elf::isMaster(getZZ())) {
    return TelegramType::MasterMaster;
  }
  return TelegramType::MasterSlave;
}

int16_t TelegramBase::getRequestByte(uint8_t pos) {
  if (pos > getNN() || pos >= EBUS_MAX_DATA_LENGTH) {
    return -1;
  }
  return requestBuffer[EBUS_OFFSET_DATA + pos];
}

uint8_t TelegramBase::getRequestCRC() {
  return requestBuffer[EBUS_OFFSET_DATA + getNN()];
}

void TelegramBase::pushReqData(uint8_t cr) {
  pushBuffer(cr, requestBuffer, &requestBufferPos, &requestRollingCRC, EBUS_OFFSET_DATA + getNN());
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
