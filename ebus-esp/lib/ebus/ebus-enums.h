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
  endCompleted = -16,
  endAbort = -20,
};

enum SendCommandState : int8_t {
  waitForSend = 1,
  waitForSendArbitration1st = 6,
  waitForSendArbitration2nd = 7,
  waitForCommandAck = 8,
  endSendCompleted = -16,
  endSendFailed = -17,
};

}  // namespace Ebus

#endif