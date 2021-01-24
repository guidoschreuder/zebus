#include <ebus.h>

namespace Ebus {

Telegram::Telegram() {
}

void Telegram::pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
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

TelegramState Telegram::getState() {
  return state;
}

void Telegram::setState(TelegramState newState) {
  state = newState;
}

int16_t Telegram::getRequestByte(uint8_t pos) {
  if (pos >= getNN()) {
    return -1;
  }
  return requestBuffer[OFFSET_DATA + pos];
}

uint8_t Telegram::getRequestCRC() {
  return requestBuffer[OFFSET_DATA + getNN()];
}

int16_t Telegram::getResponseByte(uint8_t pos) {
  if (pos >= getResponseNN()) {
    return -1;
  }
  return responseBuffer[RESPONSE_OFFSET + pos];
}

uint8_t Telegram::getResponseCRC() {
  return responseBuffer[RESPONSE_OFFSET + getResponseNN()];
}

void Telegram::pushReqData(uint8_t cr) {
  pushBuffer(cr, requestBuffer, &requestBufferPos, &requestRollingCRC, OFFSET_DATA + getNN());
}
void Telegram::pushRespData(uint8_t cr) {
  pushBuffer(cr, responseBuffer, &responseBufferPos, &responseRollingCRC, RESPONSE_OFFSET + getResponseNN());
}

TelegramType Telegram::getType() {
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

bool Telegram::isAckExpected() {
  return (getType() != TelegramType::Broadcast);
}

bool Telegram::isResponseExpected() {
  return (getType() == TelegramType::MasterSlave);
}

bool Telegram::isRequestComplete() {
  return (requestBufferPos > OFFSET_DATA) && (requestBufferPos == (OFFSET_DATA + getNN() + 1)) && !waitForEscaped;
}

bool Telegram::isRequestValid() {
  return isRequestComplete() && getRequestCRC() == requestRollingCRC;
}

bool Telegram::isResponseComplete() {
  return (responseBufferPos > RESPONSE_OFFSET) && (responseBufferPos == (RESPONSE_OFFSET + getResponseNN() + 1)) && !waitForEscaped;
}

bool Telegram::isResponseValid() {
  return isResponseComplete() && getResponseCRC() == responseRollingCRC;
}

bool Telegram::isFinished() {
  return state < TelegramState::unknown;
}

Ebus::Ebus(uint8_t master) {
  masterAddress = master;
}

void Ebus::setUartSendFunction(void (*uart_send)(const char *, int16_t)) {
  uartSend = uart_send;
}
void Ebus::setQueueHistoricFunction(void (*queue_historic)(Telegram)) {
  queueHistoric = queue_historic;
}

void IRAM_ATTR Ebus::processReceivedChar(int cr) {
  if (activeTelegram.isFinished()) {
    if (queueHistoric) {
      queueHistoric(activeTelegram);
    }
    activeTelegram = Telegram();
  }

  if (cr < 0) {
    activeTelegram.setState(TelegramState::endAbort);
    return;
  }

  uint8_t receivedByte = (uint8_t)cr;

  switch (activeTelegram.getState()) {
  case TelegramState::waitForSyn:
    if (receivedByte == SYN) {
      activeTelegram.setState(TelegramState::waitForRequestData);
    }
    break;
  case TelegramState::waitForRequestData:
    if (receivedByte == SYN) {
      //       g_activeTelegram.state = EbusTelegram::State::endErrorUnexpectedSyn;
    } else {
      activeTelegram.pushReqData(receivedByte);
      if (activeTelegram.isRequestComplete()) {
        activeTelegram.setState(activeTelegram.isAckExpected() ? TelegramState::waitForRequestAck : TelegramState::endCompleted);
      }
    }
    break;
  case TelegramState::waitForRequestAck:
    switch (cr) {
    case ACK:
      activeTelegram.setState(activeTelegram.isResponseExpected() ? TelegramState::waitForResponseData : TelegramState::endCompleted);
      break;
    case NACK:
      activeTelegram.setState(TelegramState::endErrorRequestNackReceived);
      break;
    default:
      activeTelegram.setState(TelegramState::endErrorRequestNoAck);
    }
    break;
  case TelegramState::waitForResponseData:
    if (receivedByte == SYN) {
      activeTelegram.setState(TelegramState::endErrorUnexpectedSyn);
    } else {
      activeTelegram.pushRespData(receivedByte);
      if (activeTelegram.isResponseComplete()) {
        activeTelegram.setState(TelegramState::waitForResponseAck);
      }
    }
    break;
  case TelegramState::waitForResponseAck:
    switch (cr) {
    case ACK:
      activeTelegram.setState(TelegramState::endCompleted);
      break;
    case NACK:
      activeTelegram.setState(TelegramState::endErrorResponseNackReceived);
      break;
    default:
      activeTelegram.setState(TelegramState::endErrorResponseNoAck);
    }
    break;
  default:
    break;
  }

  if (activeTelegram.getState() == TelegramState::waitForRequestAck &&
      activeTelegram.getZZ() == EBUS_SLAVE_ADDRESS(masterAddress)) {
    char buf[RESPONSE_BUFFER_SIZE] = {0};
    int len = 0;
    // we are requested to respond
    if (activeTelegram.isRequestValid()) {
      // TODO: below needs to be refactored to be more generic
      // request to identification request
      if (activeTelegram.getPB() == 0x07 &&
          activeTelegram.getSB() == 0x04) {
        buf[len++] = ACK;
        uint8_t fixedResponse[] = {0xA, 0xDD, 0x47, 0x75, 0x69, 0x64, 0x6F, 0x01, 0x02, 0x03, 0x04, 0x31};
        int i;
        for (int i = 0; i < sizeof(fixedResponse) / sizeof(uint8_t); i++) {
          buf[len++] = (uint8_t) fixedResponse[i];
        }
      }
    } else {
      buf[len++] = NACK;
    }
    // only ACK known commands
    if (len) {
      uartSend(buf, len);
    }
  }
}

#ifdef UNIT_TEST
Telegram Ebus::getActiveTelegram() {
  return activeTelegram;
}
#endif

unsigned char Ebus::Elf::crc8Calc(unsigned char data, unsigned char crc_init) {
  unsigned char crc;
  unsigned char polynom;
  int i;

  crc = crc_init;
  for (i = 0; i < 8; i++) {
    if (crc & 0x80) {
      polynom = (unsigned char)0x9B;
    } else {
      polynom = (unsigned char)0;
    }
    crc = (unsigned char)((crc & ~0x80) << 1);
    if (data & 0x80) {
      crc = (unsigned char)(crc | 1);
    }
    crc = (unsigned char)(crc ^ polynom);
    data = (unsigned char)(data << 1);
  }
  return (crc);
}

unsigned char Ebus::Elf::crc8Array(unsigned char data[], unsigned int length) {
  int i;
  unsigned char uc_crc;
  uc_crc = (unsigned char)0;
  for (i = 0; i < length; i++) {
    uc_crc = crc8Calc(data[i], uc_crc);
  }
  return (uc_crc);
}

bool Ebus::Elf::isMaster(uint8_t address) {
  return isMasterNibble(getPriorityClass(address)) &&  //
         isMasterNibble(getSubAddress(address));
}

int Ebus::Elf::isMasterNibble(uint8_t nibble) {
  switch (nibble) {
  case 0b0000:
  case 0b0001:
  case 0b0011:
  case 0b0111:
  case 0b1111:
    return true;
  default:
    return false;
  }
}

uint8_t Ebus::Elf::getPriorityClass(uint8_t address) {
  return (address & 0x0F);
}

uint8_t Ebus::Elf::getSubAddress(uint8_t address) {
  return (address >> 4);
}

}  // namespace Ebus
