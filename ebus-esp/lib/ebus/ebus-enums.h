#ifndef _EBUS_ENUMS
#define _EBUS_ENUMS

#include <stdint.h>

namespace Ebus {

enum EbusState : int8_t {
  normal,
  arbitration,
};

enum TelegramType : int8_t {
  Unknown = -1,
  Broadcast = 0,
  MasterMaster = 1,
  MasterSlave = 2,
};

enum TelegramState : int8_t {
  waitForSyn = 1,  // no SYN seen yet
  waitForSend = 2,
  waitForRequestData = 3,
  waitForRequestAck = 4,
  waitForResponseData = 5,
  waitForResponseAck = 6,
  waitForArbitration = 7,
  waitForArbitration2nd = 8,
  waitForCommandAck = 9,
  unknown = 0,
  endErrorUnexpectedSyn = -1,
  endErrorRequestNackReceived = -2,
  endErrorResponseNackReceived = -3,
  endErrorResponseNoAck = -4,
  endErrorRequestNoAck = -4,
  endArbitration = -5,
  endCompleted = -16,
  endSendFailed = -17,
};

}  // namespace Ebus

#endif