#include <Ebus.h>

namespace Ebus {

Ebus::Ebus(uint8_t master, uint8_t max_tries) {
  masterAddress = master;
  maxTries = max_tries;
}

void Ebus::setUartSendFunction(void (*uart_send)(const char *, int16_t)) {
  uartSend = uart_send;
}

void Ebus::setQueueHistoricFunction(void (*queue_historic)(Telegram)) {
  queueHistoric = queue_historic;
}

void Ebus::setDeueueCommandFunction(bool (*dequeue_command)(void *const command)) {
  dequeueCommand = dequeue_command;
}

void Ebus::uartSendChar(uint8_t cr) {
  const char buffer[1] = {(char) cr};
  uartSend(buffer, 1);
}

void Ebus::uartSendRemainingRequestPart(SendCommand command) {
  uartSendChar(command.getZZ());
  uartSendChar(command.getPB());
  uartSendChar(command.getSB());
  uartSendChar(command.getNN());
  // NOTE: use <= in loop, so we also get CRC
  for (int i = 0; i <= command.getNN(); i++) {
    uint8_t el = (uint8_t) command.getRequestByte(i);
    if (el == ESC) {
      uartSendChar(ESC);
      uartSendChar(0x00);
    } else if (el == SYN) {
      uartSendChar(ESC);
      uartSendChar(0x01);
    } else {
      uartSendChar(el);
    }
  }
}

void IRAM_ATTR Ebus::processReceivedChar(int cr) {
  // keep track of number of character between last 2 SYN chars
  // this is needed in case of arbitration
  if (cr == SYN) {
    state = charCountSinceLastSyn == 1 ? EbusState::arbitration : EbusState::normal;
    charCountSinceLastSyn = 0;
  } else {
    charCountSinceLastSyn++;
  }

  if (receivingTelegram.isFinished()) {
    if (queueHistoric) {
      queueHistoric(receivingTelegram);
    }
    receivingTelegram = Telegram();
  }

  if ((activeCommand.getState() < 0) && dequeueCommand) {
    SendCommand dequeued;
    if (dequeueCommand(&dequeued)) {
      activeCommand = dequeued;
    }
  }

  if (cr < 0) {
    receivingTelegram.setState(TelegramState::endAbort);
    return;
  }

  uint8_t receivedByte = (uint8_t)cr;

  switch (receivingTelegram.getState()) {
  case TelegramState::waitForSyn:
    if (receivedByte == SYN) {
      receivingTelegram.setState(TelegramState::waitForArbitration);
    }
    break;
  case TelegramState::waitForArbitration:
    if (receivedByte != SYN) {
      receivingTelegram.pushReqData(receivedByte);
      receivingTelegram.setState(TelegramState::waitForRequestData);
    }
    break;
  case TelegramState::waitForRequestData:
    if (receivedByte == SYN) {
      if (receivingTelegram.getZZ() == ESC) {
        receivingTelegram.setState(TelegramState::endArbitration);
      } else {
        receivingTelegram.setState(TelegramState::endErrorUnexpectedSyn);
      }
    } else {
      receivingTelegram.pushReqData(receivedByte);
      if (receivingTelegram.isRequestComplete()) {
        receivingTelegram.setState(receivingTelegram.isAckExpected() ? TelegramState::waitForRequestAck : TelegramState::endCompleted);
      }
    }
    break;
  case TelegramState::waitForRequestAck:
    switch (cr) {
    case ACK:
      receivingTelegram.setState(receivingTelegram.isResponseExpected() ? TelegramState::waitForResponseData : TelegramState::endCompleted);
      break;
    case NACK:
      receivingTelegram.setState(TelegramState::endErrorRequestNackReceived);
      break;
    default:
      receivingTelegram.setState(TelegramState::endErrorRequestNoAck);
    }
    break;
  case TelegramState::waitForResponseData:
    if (receivedByte == SYN) {
      receivingTelegram.setState(TelegramState::endErrorUnexpectedSyn);
    } else {
      receivingTelegram.pushRespData(receivedByte);
      if (receivingTelegram.isResponseComplete()) {
        receivingTelegram.setState(TelegramState::waitForResponseAck);
      }
    }
    break;
  case TelegramState::waitForResponseAck:
    switch (cr) {
    case ACK:
      receivingTelegram.setState(TelegramState::endCompleted);
      break;
    case NACK:
      receivingTelegram.setState(TelegramState::endErrorResponseNackReceived);
      break;
    default:
      receivingTelegram.setState(TelegramState::endErrorResponseNoAck);
    }
    break;
  default:
    break;
  }

  switch (activeCommand.getState()) {
  case SendCommandState::waitForSend:
    if (cr == SYN && state == EbusState::normal) {
      activeCommand.setState(SendCommandState::waitForSendArbitration1st);
      uartSendChar(activeCommand.getQQ());
    }
    break;
  case SendCommandState::waitForSendArbitration1st:
    if (cr == activeCommand.getQQ()) {
      // we won arbitration
      uartSendRemainingRequestPart(activeCommand);
      activeCommand.setState(activeCommand.isAckExpected() ? SendCommandState::waitForCommandAck : SendCommandState::endSendCompleted);
    } else if (Elf::getPriorityClass(cr) == Elf::getPriorityClass(activeCommand.getQQ())) {
      // eligible for round 2
      activeCommand.setState(SendCommandState::waitForSendArbitration2nd);
    } else {
      // lost arbitration, try again later if retries left
      activeCommand.setState(activeCommand.canRetry(maxTries) ? SendCommandState::waitForSend : SendCommandState::endSendFailed);
    }
    break;
  case SendCommandState::waitForSendArbitration2nd:
    if (cr == SYN) {
      uartSendChar(activeCommand.getQQ());
    } else if (cr == activeCommand.getQQ()) {
      // won round 2
      uartSendRemainingRequestPart(activeCommand);
      activeCommand.setState(activeCommand.isAckExpected() ? SendCommandState::waitForCommandAck : SendCommandState::endSendCompleted);
    } else {
      // try again later if retries left
      activeCommand.setState(activeCommand.canRetry(maxTries) ? SendCommandState::waitForSend : SendCommandState::endSendFailed);
    }
    break;
  case SendCommandState::waitForCommandAck:
    if (cr == ACK) {
      activeCommand.setState(SendCommandState::endSendCompleted);
    } else {
      activeCommand.setState(activeCommand.canRetry(maxTries) ? SendCommandState::waitForSend : SendCommandState::endSendFailed);
    }
    break;
  default:
    break;
  }

  if (receivingTelegram.getState() == TelegramState::waitForRequestAck &&
      receivingTelegram.getZZ() == EBUS_SLAVE_ADDRESS(masterAddress)) {
    char buf[RESPONSE_BUFFER_SIZE] = {0};
    int len = 0;
    // we are requested to respond
    if (receivingTelegram.isRequestValid()) {
      // TODO: below needs to be refactored to be more generic
      // request to identification request
      if (receivingTelegram.getPB() == 0x07 &&
          receivingTelegram.getSB() == 0x04) {
        buf[len++] = ACK;
        uint8_t fixedResponse[] = {0xA, 0xDD, 0x47, 0x75, 0x69, 0x64, 0x6F, 0x01, 0x02, 0x03, 0x04, 0x31};
        for (int i = 0; i < sizeof(fixedResponse) / sizeof(uint8_t); i++) {
          buf[len++] = (uint8_t)fixedResponse[i];
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
Telegram Ebus::getReceivingTelegram() {
  return receivingTelegram;
}
#endif

unsigned char Ebus::Elf::crc8Calc(unsigned char data, unsigned char crc_init) {
  unsigned char crc;
  unsigned char polynom;

  crc = crc_init;
  for (int i = 0; i < 8; i++) {
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
  unsigned char uc_crc;
  uc_crc = (unsigned char)0;
  for (int i = 0; i < length; i++) {
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
