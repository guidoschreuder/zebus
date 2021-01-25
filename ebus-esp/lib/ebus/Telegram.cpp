#include "Telegram.h"

namespace Ebus {

Telegram::Telegram() {
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

void Telegram::pushRespData(uint8_t cr) {
  pushBuffer(cr, responseBuffer, &responseBufferPos, &responseRollingCRC, RESPONSE_OFFSET + getResponseNN());
}

bool Telegram::isResponseComplete() {
  return (getState() > TelegramState::waitForSyn || getState() == TelegramState::endCompleted) && (responseBufferPos > RESPONSE_OFFSET) && (responseBufferPos == (RESPONSE_OFFSET + getResponseNN() + 1)) && !waitForEscaped;
}

bool Telegram::isResponseValid() {
  return isResponseComplete() && getResponseCRC() == responseRollingCRC;
}

}