#ifndef EBUS_H
#define EBUS_H

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

typedef uint8_t (*send_response_handler)(Ebus::Telegram &, uint8_t *);

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
  std::list<send_response_handler> sendResponseHandlers;

  void (*uartSend)(const char *, int16_t);
  void (*queueReceivedTelegram)(Telegram &);
  bool (*dequeueCommand)(void *const command);
  uint8_t uartSendChar(uint8_t cr, bool esc, bool runCrc, uint8_t crc_init);
  void uartSendChar(uint8_t cr, bool esc = true);
  void uartSendRemainingRequestPart(SendCommand &command);
  void handleResponse(Telegram &telegram);

  public:
  explicit Ebus(ebus_config_t &config);
  void setUartSendFunction(void (*uartSend)(const char *, int16_t size));
  void setQueueReceivedTelegramFunction(void (*queue_received_telegram)(Telegram &telegram));
  void setDeueueCommandFunction(bool (*dequeue_command)(void *const command));
  void processReceivedChar(unsigned char receivedByte);
  void addSendResponseHandler(send_response_handler);

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
