#ifndef __EBUS
#define __EBUS

#include <stdint.h>

#ifdef __NATIVE
#include "mock-queue.h"
extern Queue telegramHistory;
#else
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#endif

// for testing in native unit test we do not have ESP32 so fake it here
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#define EBUS_SLAVE_ADDRESS(MASTER) (((MASTER) + 5) % 0xFF)

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

unsigned char crc8_calc(unsigned char data, unsigned char crc_init);
unsigned char crc8_array(unsigned char data[], unsigned int length);

bool is_master(uint8_t address);
void IRAM_ATTR ebus_process_received_char(int cr);

void IRAM_ATTR new_active_telegram();

namespace Ebus {

enum TelegramType : int8_t {
  Unknown = -1,
  Broadcast = 0,
  MasterMaster = 1,
  MasterSlave = 2,
};
enum TelegramState : int8_t {
  waitForSyn = 1,  // no SYN seen yet
  waitForRequestData = 2,
  waitForRequestAck = 3,
  waitForResponseData = 4,
  waitForResponseAck = 5,
  unknown = 0,
  endErrorUnexpectedSyn = -1,
  endErrorRequestNackReceived = -2,
  endErrorResponseNackReceived = -3,
  endErrorResponseNoAck = -4,
  endErrorRequestNoAck = -4,
  endCompleted = -32,
  endAbort = -99,
};

class Telegram {
  TelegramState state = TelegramState::waitForSyn;
  bool waitForEscaped = false;
  uint8_t requestBuffer[REQUEST_BUFFER_SIZE] = {ESC, ESC};  // initialize QQ and ZZ with ESC char to distinguish from valid master 0
  uint8_t requestBufferPos = 0;
  uint8_t requestRollingCRC = 0;
  uint8_t responseBuffer[RESPONSE_BUFFER_SIZE] = {0};
  uint8_t responseBufferPos = 0;
  uint8_t responseRollingCRC = 0;
  void pushBuffer(uint8_t cr, uint8_t *buffer, uint8_t *pos, uint8_t *crc, int max_pos);
  TelegramType getType();

  public:
  Telegram();

  _GETTER(requestBuffer, QQ);
  _GETTER(requestBuffer, ZZ);
  _GETTER(requestBuffer, PB);
  _GETTER(requestBuffer, SB);
  _GETTER(requestBuffer, NN);
  uint8_t getResponseNN() {
    return responseBuffer[0];
  }

  TelegramState getState();
  void setState(TelegramState newState);

  int16_t getRequestByte(uint8_t pos);
  uint8_t getRequestCRC();
  int16_t getResponseByte(uint8_t pos);
  uint8_t getResponseCRC();

  void pushReqData(uint8_t cr);
  void pushRespData(uint8_t cr);
  bool isAckExpected();
  bool isResponseExpected();
  bool isRequestComplete();
  bool isRequestValid();
  bool isResponseComplete();
  bool isResponseValid();
  bool isFinished();
};
}  // namespace Ebus

extern Ebus::Telegram g_activeTelegram;

#ifdef __NATIVE
extern Queue telegramHistoryMockQueue;
#else
extern QueueHandle_t telegramHistoryQueue;
#endif

#endif
