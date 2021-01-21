#ifndef __EBUS
#define __EBUS

#include <stddef.h>
#include <stdint.h>
#include <queue.h>

#include <cstdio>  //debug

// for testing in native unit test we do not have ESP32 so fake it here
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#define EBUS_SLAVE_ADDRESS(MASTER) (((MASTER) + 5) % 0xFF)
#define EBUS_SERIAL_BUFFER_SIZE 64

#define SYN 0xAA
#define ESC 0xA9
#define ACK 0x00
#define NACK 0xFF

#define BROADCAST_ADDRESS 0xFE

#define MAX_DATA_LENGTH 16

#define OFFSET_QQ 0
#define OFFSET_ZZ 1
#define OFFSET_PB 2
#define OFFSET_SB 3
#define OFFSET_NN 4
#define OFFSET_DATA 5
#define REQUEST_BUFFER_SIZE (OFFSET_DATA + MAX_DATA_LENGTH + 1)
#define RESPONSE_BUFFER_SIZE (MAX_DATA_LENGTH + 2)
#define RESPONSE_OFFSET 1

#define _GETTER(BUFFER, POS)     \
  uint8_t get##POS() {           \
    return BUFFER[OFFSET_##POS]; \
  }

// ring buffer for debug
extern uint8_t g_serialBuffer[EBUS_SERIAL_BUFFER_SIZE];
extern volatile uint32_t g_serialBuffer_pos;

unsigned char crc8_calc(unsigned char data, unsigned char crc_init);
unsigned char crc8_array(unsigned char data[], unsigned int length);

bool is_master(uint8_t address);
void IRAM_ATTR process_received(int cr);

struct EbusTelegram {
  enum Type {
    Unknown = -1,
    Broadcast = 0,
    MasterMaster = 1,
    MasterSlave = 2,
  };
  enum EbusState {
    waitForSyn = 1,  // no SYN seen yet
    waitForRequestData = 2,
    waitForRequestAck = 3,
    waitForResponseData = 4,
    waitForResponseAck = 5,
    endCompleted = 0,
    endErrorUnexpectedSyn = -1,
    endErrorRequestNackReceived = -2,
    endErrorResponseNackReceived = -3,
    endErrorInvalidCharacterReceived = -4,
    endAbort = -99
  };
  bool waitForEscaped = false;
  int8_t state = EbusState::waitForSyn;
  int8_t type = Type::Unknown;
  uint8_t requestBuffer[REQUEST_BUFFER_SIZE] = {0xAA};
  uint8_t requestBufferPos = 0;
  uint8_t requestRollingCRC = 0;
  uint8_t responseBuffer[RESPONSE_BUFFER_SIZE];
  uint8_t responseBufferPos = 0;
  uint8_t responseRollingCRC = 0;

  void push_req_data(uint8_t cr) {
    push_buffer(cr, requestBuffer, &requestBufferPos, &requestRollingCRC, OFFSET_DATA + getNN());
    // if (waitForEscaped) {
    //   if (requestBufferPos < OFFSET_DATA + getNN()) {
    //     requestRollingCRC = crc8_calc(cr, requestRollingCRC);
    //   }
    //   requestBuffer[requestBufferPos] = (cr == 0x0 ? ESC : SYN);
    //   waitForEscaped = false;
    // } else {
    //   if (requestBufferPos < OFFSET_DATA + getNN()) {
    //     requestRollingCRC = crc8_calc(cr, requestRollingCRC);
    //   }
    //   requestBuffer[requestBufferPos++] = cr;
    //   waitForEscaped = (cr == ESC);
    // }
  }

  void push_resp_data(uint8_t cr) {
    push_buffer(cr, responseBuffer, &responseBufferPos, &responseRollingCRC, RESPONSE_OFFSET + getResponseNN());
  }

  void push_buffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos) {
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

  _GETTER(requestBuffer, QQ);
  _GETTER(requestBuffer, ZZ);
  _GETTER(requestBuffer, PB);
  _GETTER(requestBuffer, SB);
  _GETTER(requestBuffer, NN);
  uint8_t getResponseNN() {
    return responseBuffer[0];
  }

  int8_t get_type() {
    if (getZZ() == 0xAA) {
      return Type::Unknown;
    }
    if (getZZ() == BROADCAST_ADDRESS) {
      return Type::Broadcast;
    }
    if (is_master(getZZ())) {
      return Type::MasterMaster;
    }
    return Type::MasterSlave;
  }

  bool ack_expected() {
    return (get_type() != Type::Broadcast);
  }

  bool response_expected() {
    return (get_type() == Type::MasterSlave);
  }

  uint8_t getRequestCRC() {
    return requestBuffer[OFFSET_DATA + getNN()];
  }

  bool isRequestComplete() {
    //printf("state: %d\nbuf-pos: %d\nNN: %d\nwait-for-esc: %s\n", state, requestBufferPos, getNN(), waitForEscaped ? "t":"f");
    return state >= EbusState::waitForSyn && (requestBufferPos > OFFSET_DATA) && (requestBufferPos == (OFFSET_DATA + getNN() + 1)) && !waitForEscaped;
  }

  bool isRequestValid() {
    return state >= EbusState::waitForSyn && isRequestComplete() && getRequestCRC() == requestRollingCRC;
  }

  uint8_t getResponseCRC() {
    return responseBuffer[RESPONSE_OFFSET + getResponseNN()];
  }

  bool isResponseComplete() {
    return state >= EbusState::waitForSyn && (responseBufferPos > RESPONSE_OFFSET) && (responseBufferPos == (RESPONSE_OFFSET + getResponseNN() + 1)) && !waitForEscaped;
  }

  bool isResponseValid() {
    return state >= EbusState::waitForSyn && isResponseComplete() && getResponseCRC() == responseRollingCRC;
  }

  bool isFinished() {
    return state <= EbusState::endCompleted;
  }

};

extern EbusTelegram g_activeTelegram;
extern Queue telegramHistory;
extern Queue telegramToSend;


#endif
