#include "Telegram.h"

namespace Ebus {

Telegram::Telegram() {
  state = TelegramState::waitForSyn;
}

int16_t Telegram::getResponseByte(uint8_t pos) {
  if (pos > getResponseNN() || pos >= EBUS_MAX_DATA_LENGTH) {
    return EBUS_INVALID_RESPONSE_BYTE;
  }
  return responseBuffer[EBUS_RESPONSE_OFFSET + pos];
}

uint8_t Telegram::getResponseCRC() {
  return responseBuffer[EBUS_RESPONSE_OFFSET + getResponseNN()];
}

void Telegram::pushRespData(uint8_t cr) {
  pushBuffer(cr, responseBuffer, &responseBufferPos, &responseRollingCRC, EBUS_RESPONSE_OFFSET + getResponseNN());
}

bool Telegram::isResponseComplete() {
  return (state > TelegramState::waitForSyn || state == TelegramState::endCompleted) &&
         (responseBufferPos > EBUS_RESPONSE_OFFSET) &&
         (responseBufferPos == (EBUS_RESPONSE_OFFSET + getResponseNN() + 1)) &&
         !waitForEscaped;
}

bool Telegram::isResponseValid() {
  return isResponseComplete() && getResponseCRC() == responseRollingCRC;
}

bool Telegram::isRequestComplete() {
  return (state > TelegramState::waitForSyn || state == TelegramState::endCompleted) &&
         (requestBufferPos > EBUS_OFFSET_DATA) &&
         (requestBufferPos == (EBUS_OFFSET_DATA + getNN() + 1)) && !waitForEscaped;
}
bool Telegram::isRequestValid() {
  return isRequestComplete() && getRequestCRC() == requestRollingCRC;
}

}  // namespace Ebus
