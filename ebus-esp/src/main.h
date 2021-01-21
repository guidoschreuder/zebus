#include "config.h"

// helper macros
#define CONCAT(a, b)    XCAT(a, b)
#define XCAT(a, b)      a ## b

#define UART_NUM_EBUS   CONCAT(UART_NUM_, UART_FOR_EBUS)
#define UART_EBUS       CONCAT(UART, UART_FOR_EBUS)
