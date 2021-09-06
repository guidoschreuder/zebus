#ifndef __EBUS
#define __EBUS

#include <stdint.h>

#include <list>

#include "SendCommand.h"
#include "Telegram.h"
#include "ebus-enums.h"

typedef struct {
  uint8_t master_address;
  uint8_t max_tries;
  uint8_t max_lock_counter;
} ebus_config_t;

namespace Ebus {

class Ebus {
  private:
  uint8_t masterAddress;
  uint8_t maxTries;
  uint8_t maxLockCounter;
  uint8_t lockCounter = 0;
  uint8_t charCountSinceLastSyn = 0;
  EbusState state = EbusState::arbitration;
  Telegram receivingTelegram;
  SendCommand activeCommand;
  void (*uartSend)(const char *, int16_t);
  void (*queueHistoric)(Telegram);
  bool (*dequeueCommand)(void *const command);
  void uartSendChar(uint8_t cr, bool esc = true);
  void uartSendRemainingRequestPart(SendCommand command);

  public:
  explicit Ebus(ebus_config_t config);
  void setUartSendFunction(void (*uartSend)(const char *, int16_t size));
  void setQueueHistoricFunction(void (*queue_historic)(Telegram telegram));
  void setDeueueCommandFunction(bool (*dequeue_command)(void *const command));
  void processReceivedChar(unsigned char receivedByte);

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
  Telegram getReceivingTelegram();
#endif
};

}  // namespace Ebus

#endif
