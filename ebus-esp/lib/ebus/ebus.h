#ifndef __EBUS
#define __EBUS

#include <stdint.h>

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

class Ebus {
  uint8_t masterAddress;
  void (*uartSend)(const char *, int16_t);
  void (*queueHistoric)(Telegram);
  Telegram activeTelegram;

  public:
  explicit Ebus(uint8_t master);
  void setUartSendFunction(void (*uartSend)(const char *, int16_t size));
  void setQueueHistoricFunction(void (*queue_historic)(Telegram telegram));
  void IRAM_ATTR processReceivedChar(int cr);
  class Elf {
public:
    static unsigned char crc8Calc(unsigned char data, unsigned char crc_init);
    static unsigned char crc8Array(unsigned char data[], unsigned int length);
    static bool isMaster(uint8_t address);
    static int isMasterNibble(uint8_t nibble);
    static uint8_t getPriorityClass(uint8_t address);
    static uint8_t getSubAddress(uint8_t address);

  };

#ifdef UNIT_TEST
  Telegram
  getActiveTelegram();
#endif
};

}  // namespace Ebus

#endif
