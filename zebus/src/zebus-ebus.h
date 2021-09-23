#ifndef _ZEBUS_EBUS_H
#define _ZEBUS_EBUS_H

// helper macros
#define CONCAT(a, b)    XCAT(a, b)
#define XCAT(a, b)      a ## b

#define UART_NUM_EBUS   CONCAT(UART_NUM_, EBUS_UART_NUM)
#define UART_EBUS       CONCAT(UART, EBUS_UART_NUM)

void setupEbus();
void enqueueEbusCommand(const void * const itemToQueue);

#endif
