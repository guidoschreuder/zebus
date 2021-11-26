#ifndef EBUS_ENUMS_H
#define EBUS_ENUMS_H

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

#define TELEGRAM_STATE_TABLE \
X(waitForSyn, 1)                    \
X(waitForSend, 2)                   \
X(waitForRequestData, 3)            \
X(waitForRequestAck, 4)             \
X(waitForResponseData, 5)           \
X(waitForResponseAck, 6)            \
X(waitForArbitration, 7)            \
X(waitForArbitration2nd, 8)         \
X(waitForCommandAck, 9)             \
X(unknown, 0)                       \
X(endErrorUnexpectedSyn, -1)        \
X(endErrorRequestNackReceived, -2)  \
X(endErrorResponseNackReceived, -3) \
X(endErrorResponseNoAck, -4)        \
X(endErrorRequestNoAck, -5)         \
X(endArbitration, -6)               \
X(endCompleted, -16)                \
X(endSendFailed, -17)

#define X(name, int) name = int,
enum TelegramState : int8_t {
  TELEGRAM_STATE_TABLE
};
#undef X

}  // namespace Ebus

#endif
