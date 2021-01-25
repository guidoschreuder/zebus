#include <Ebus.h>

namespace Ebus {

Ebus::Ebus(uint8_t master) {
  masterAddress = master;
}

void IRAM_ATTR Ebus::setUartSendFunction(void (*uart_send)(const char *, int16_t)) {
  uartSend = uart_send;
}

void IRAM_ATTR Ebus::setQueueHistoricFunction(void (*queue_historic)(Telegram)) {
  queueHistoric = queue_historic;
}

void IRAM_ATTR Ebus::setDeueueCommandFunction(bool (*dequeue_command)(void * const command)) {
  dequeueCommand = dequeue_command;
}

void IRAM_ATTR Ebus::processReceivedChar(int cr) {
  if (receivingTelegram.isFinished()) {
    if (queueHistoric) {
      queueHistoric(receivingTelegram);
    }
    receivingTelegram = Telegram();
  }

  // if (activeCommand.isFinished() && dequeueCommand) {
  //   // see if we can dequeue a new command
  //   dequeueCommand(&activeCommand);
  // }

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
      receivingTelegram.pushReqData(receivedByte);
      receivingTelegram.setState(TelegramState::waitForRequestData);
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
