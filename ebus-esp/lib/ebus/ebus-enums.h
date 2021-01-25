#ifndef _EBUS_ENUMS
#define _EBUS_ENUMS

#include <stdint.h>

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
  waitForArbitration = 6,
  unknown = 0,
  endErrorUnexpectedSyn = -1,
  endErrorRequestNackReceived = -2,
  endErrorResponseNackReceived = -3,
  endErrorResponseNoAck = -4,
  endErrorRequestNoAck = -4,
  endArbitration = -5,
  endCompleted = -32,
  endAbort = -99,
};

enum SendCommandState : int8_t {
  waitForSend = 1,
  waitForSendArbitration = 6,
  endSendCompleted = -32,
};

}

#endif