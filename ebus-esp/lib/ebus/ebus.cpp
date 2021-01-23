#include <ebus.h>
#include <stdlib.h>

#include <cstring>

EbusTelegram g_activeTelegram;
static const struct EbusTelegram EmptyTelegram;

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
  EbusTelegram* copy = (EbusTelegram*)malloc(sizeof(EbusTelegram));
  memcpy(copy, &g_activeTelegram, sizeof(struct EbusTelegram));
  telegramHistoryMockQueue.enqueue(copy);
#else

  BaseType_t xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendToBackFromISR(telegramHistoryQueue, &g_activeTelegram, &xHigherPriorityTaskWoken);

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
#endif

  g_activeTelegram = EmptyTelegram;
}

void IRAM_ATTR ebus_process_received_char(int cr) {
  uint8_t receivedByte = (uint8_t)cr;

  if (g_activeTelegram.isFinished()) {
    new_active_telegram();
  }

  switch (g_activeTelegram.state) {
  case EbusTelegram::State::waitForSyn:
    if (receivedByte == SYN) {
      g_activeTelegram.state = EbusTelegram::State::waitForRequestData;
    }
    break;
  case EbusTelegram::State::waitForRequestData:
    if (receivedByte == SYN) {
      //       g_activeTelegram.state = EbusTelegram::State::endErrorUnexpectedSyn;
    } else {
      g_activeTelegram.push_req_data(receivedByte);
      if (g_activeTelegram.isRequestComplete()) {
        g_activeTelegram.state = g_activeTelegram.isAckExpected() ? EbusTelegram::State::waitForRequestAck : EbusTelegram::State::endCompleted;
      }
    }
    break;
  case EbusTelegram::State::waitForRequestAck:
    switch (cr) {
    case ACK:
      g_activeTelegram.state = g_activeTelegram.isResponseExpected() ? EbusTelegram::State::waitForResponseData : EbusTelegram::State::endCompleted;
      break;
    case NACK:
      g_activeTelegram.state = EbusTelegram::State::endErrorRequestNackReceived;
      break;
    default:
      g_activeTelegram.state = EbusTelegram::State::endErrorRequestNoAck;
    }
    break;
  case EbusTelegram::State::waitForResponseData:
    if (receivedByte == SYN) {
      g_activeTelegram.state = EbusTelegram::State::endErrorUnexpectedSyn;
    } else {
      g_activeTelegram.push_resp_data(receivedByte);
      if (g_activeTelegram.isResponseComplete()) {
        g_activeTelegram.state = EbusTelegram::State::waitForResponseAck;
      }
    }
    break;
  case EbusTelegram::State::waitForResponseAck:
    switch (cr) {
    case ACK:
      g_activeTelegram.state = EbusTelegram::State::endCompleted;
      break;
    case NACK:
      g_activeTelegram.state = EbusTelegram::State::endErrorResponseNackReceived;
      break;
    default:
      g_activeTelegram.state = EbusTelegram::State::endErrorResponseNoAck;
    }
    break;
  }
}
