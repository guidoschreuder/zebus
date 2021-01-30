#include "Telegram.h"

namespace Ebus {

class EbusListener {
  public:
  virtual bool handle(Telegram telegram) = 0;
};

}  // namespace Ebus
