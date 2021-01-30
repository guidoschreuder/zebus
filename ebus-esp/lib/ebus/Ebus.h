#ifndef __EBUS
#define __EBUS

#include <stdint.h>

#include <list>

#include "EbusListener.h"
#include "SendCommand.h"
#include "Telegram.h"
#include "ebus-enums.h"

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
  std::list<EbusListener *> listeners;
  void (*uartSend)(const char *, int16_t);
  void (*queueHistoric)(Telegram);
  bool (*dequeueCommand)(void *const command);
  void uartSendChar(uint8_t cr);
  void uartSendRemainingRequestPart(SendCommand command);

  public:
  explicit Ebus(uint8_t master, uint8_t max_tries, uint8_t max_lock_counter);
  void setUartSendFunction(void (*uartSend)(const char *, int16_t size));
  void setQueueHistoricFunction(void (*queue_historic)(Telegram telegram));
  void setDeueueCommandFunction(bool (*dequeue_command)(void *const command));
  void processReceivedChar(int cr);
  void addListener(EbusListener *listener);
  void notifyAll(Telegram telegram);
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
