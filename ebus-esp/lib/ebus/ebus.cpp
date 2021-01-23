#include <ebus.h>
#include <stdlib.h>

#include <cstring>

Ebus::Telegram g_activeTelegram = Ebus::Telegram();

#ifdef __NATIVE
Queue telegramHistoryMockQueue(5, Queue::OnFull::removeOldest);
#endif

unsigned char crc8_calc(unsigned char data, unsigned char crc_init) {
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

unsigned char crc8_array(unsigned char data[], unsigned int length) {
  int i;
  unsigned char uc_crc;
  uc_crc = (unsigned char)0;
  for (i = 0; i < length; i++) {
    uc_crc = crc8_calc(data[i], uc_crc);
  }
  return (uc_crc);
}

namespace Ebus {

Telegram::Telegram() {

}

void Telegram::pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
  if (waitForEscaped) {
    if (*pos < max_pos) {
      *crc = crc8_calc(cr, *crc);
    }
    buffer[(*pos)] = (cr == 0x0 ? ESC : SYN);
    waitForEscaped = false;
  } else {
    if (*pos < max_pos) {
      *crc = crc8_calc(cr, *crc);
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
  if (is_master(getZZ())) {
    return TelegramType::MasterMaster;
  }
  return TelegramType::MasterSlave;
}

bool Telegram::isAckExpected() {
  return (getType() != TelegramType::Broadcast);
}

bool Ebus::Telegram::isResponseExpected() {
  return (getType() == TelegramType::MasterSlave);
}

bool Telegram::isRequestComplete() {
  return state >= TelegramState::waitForSyn && (requestBufferPos > OFFSET_DATA) && (requestBufferPos == (OFFSET_DATA + getNN() + 1)) && !waitForEscaped;
}

bool Telegram::isRequestValid() {
  return isRequestComplete() && getRequestCRC() == requestRollingCRC;
}

bool Telegram::isResponseComplete() {
  return state >= TelegramState::waitForSyn && (responseBufferPos > RESPONSE_OFFSET) && (responseBufferPos == (RESPONSE_OFFSET + getResponseNN() + 1)) && !waitForEscaped;
}

bool Telegram::isResponseValid() {
  return isResponseComplete() && getResponseCRC() == responseRollingCRC;
}

bool Telegram::isFinished() {
  return state < 0;
}

}  // namespace Ebus

int is_master_nibble(uint8_t nibble) {
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

uint8_t get_priority_class(uint8_t address) {
  return (address & 0x0F);
}

uint8_t get_sub_address(uint8_t address) {
  return (address >> 4);
}

bool is_master(uint8_t address) {
  return is_master_nibble(get_priority_class(address)) &&  //
         is_master_nibble(get_sub_address(address));
}

void IRAM_ATTR new_active_telegram() {
#ifdef __NATIVE
  Ebus::Telegram *copy = (Ebus::Telegram *)malloc(sizeof(Ebus::Telegram));
  memcpy(copy, &g_activeTelegram, sizeof(Ebus::Telegram));
  telegramHistoryMockQueue.enqueue(copy);
#else

  BaseType_t xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendToBackFromISR(telegramHistoryQueue, &g_activeTelegram, &xHigherPriorityTaskWoken);

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
#endif

  g_activeTelegram = Ebus::Telegram();
}

void IRAM_ATTR ebus_process_received_char(int cr) {
  uint8_t receivedByte = (uint8_t)cr;

  if (g_activeTelegram.isFinished()) {
    new_active_telegram();
  }

  switch (g_activeTelegram.getState()) {
  case Ebus::TelegramState::waitForSyn:
    if (receivedByte == SYN) {
      g_activeTelegram.setState(Ebus::TelegramState::waitForRequestData);
    }
    break;
  case Ebus::TelegramState::waitForRequestData:
    if (receivedByte == SYN) {
      //       g_activeTelegram.state = EbusTelegram::State::endErrorUnexpectedSyn;
    } else {
      g_activeTelegram.pushReqData(receivedByte);
      if (g_activeTelegram.isRequestComplete()) {
        g_activeTelegram.setState(g_activeTelegram.isAckExpected() ? Ebus::TelegramState::waitForRequestAck : Ebus::TelegramState::endCompleted);
      }
    }
    break;
  case Ebus::TelegramState::waitForRequestAck:
    switch (cr) {
    case ACK:
      g_activeTelegram.setState(g_activeTelegram.isResponseExpected() ? Ebus::TelegramState::waitForResponseData : Ebus::TelegramState::endCompleted);
      break;
    case NACK:
      g_activeTelegram.setState(Ebus::TelegramState::endErrorRequestNackReceived);
      break;
    default:
      g_activeTelegram.setState(Ebus::TelegramState::endErrorRequestNoAck);
    }
    break;
  case Ebus::TelegramState::waitForResponseData:
    if (receivedByte == SYN) {
      g_activeTelegram.setState(Ebus::TelegramState::endErrorUnexpectedSyn);
    } else {
      g_activeTelegram.pushRespData(receivedByte);
      if (g_activeTelegram.isResponseComplete()) {
        g_activeTelegram.setState(Ebus::TelegramState::waitForResponseAck);
      }
    }
    break;
  case Ebus::TelegramState::waitForResponseAck:
    switch (cr) {
    case ACK:
      g_activeTelegram.setState(Ebus::TelegramState::endCompleted);
      break;
    case NACK:
      g_activeTelegram.setState(Ebus::TelegramState::endErrorResponseNackReceived);
      break;
    default:
      g_activeTelegram.setState(Ebus::TelegramState::endErrorResponseNoAck);
    }
    break;
  default:
    break;
  }
}
