
#ifndef UNIT_TEST

#include "main.h"
#include "ebus-display.h"
#include "ebus-messages.h"
#include "ebus-wifi.h"

#include <soc/uart_reg.h>
#include <soc/uart_struct.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "Ebus.h"

#define CHECK_INT_STATUS(ST, MASK) (((ST) & (MASK)) == (MASK))

// EBus variables
QueueHandle_t telegramHistoryQueue;
StaticQueue_t telegramHistoryQueueBuffer;
uint8_t telegramHistoryQueueStorage[EBUS_TELEGRAM_HISTORY_QUEUE_SIZE * sizeof(Ebus::Telegram)];
QueueHandle_t telegramCommandQueue;
StaticQueue_t telegramCommandQueueBuffer;
uint8_t telegramCommandQueueStorage[EBUS_TELEGRAM_SEND_QUEUE_SIZE * sizeof(Ebus::Telegram)];

ebus_config_t ebus_config = ebus_config_t {
  .master_address = EBUS_MASTER_ADDRESS,
  .max_tries = EBUS_MAX_TRIES,
  .max_lock_counter = EBUS_MAX_LOCK_COUNTER,
};

Ebus::Ebus ebus = Ebus::Ebus(ebus_config);

struct info_t* system_info = new info_t();

void debugLogger(Ebus::Telegram telegram) {
  printf(
    "===========\nstate: %d\nQQ: %02X\tZZ: %02X\tPB: %02X\tSB: %02X\nreq(size: %d, CRC: %02x): ",  //
    telegram.getState(),
    telegram.getQQ(),
    telegram.getZZ(),
    telegram.getPB(),
    telegram.getSB(),
    telegram.getNN(),
    telegram.getResponseCRC());
  for (int i = 0; i < telegram.getNN(); i++) {
     printf(" %02X", telegram.getRequestByte(i));
  }
  printf("\n");
  if (telegram.isResponseExpected()) {
    printf("resp(size: %d, CRC: %02x): ", telegram.getResponseNN(), telegram.getResponseCRC());
     for (int i = 0; i < telegram.getResponseNN(); i++) {
        printf(" %02X", telegram.getResponseByte(i));
      }
      printf("\n");
    }
}

void  tftLogger(Ebus::Telegram telegram) {
  if (telegram.getPB() == 0x07 && telegram.getSB() == 0x04 && telegram.isResponseExpected() && telegram.isResponseValid()) {
    for (int i = 1; i < 6; i++) {
     tft.print((char)telegram.getResponseByte(i));
    }
    tft.print(" ");
  }
}

static void IRAM_ATTR ebus_uart_intr_handle(void *arg) {
  uint16_t status = UART_EBUS.int_st.val;  // read UART interrupt Status
  if (status == 0) {
    return;
  }

  uint16_t rx_fifo_len = UART_EBUS.status.rxfifo_cnt;  // read number of bytes in UART buffer
  while (rx_fifo_len) {
    ebus.processReceivedChar(UART_EBUS.fifo.rw_byte);
    rx_fifo_len--;
  }

  if (CHECK_INT_STATUS(status, UART_SW_XON_INT_ST)) {
    uart_clear_intr_status(UART_NUM_EBUS, UART_SW_XON_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_FRM_ERR_INT_ST)) {
    // process error (Triggered when the receiver detects a data frame error)
    ebus.processReceivedChar(-1);
    uart_clear_intr_status(UART_NUM_EBUS, UART_FRM_ERR_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_BRK_DET_INT_ST)) {
    // process error (Triggered when the receiver detects a 0 level after the STOP bit)
    ebus.processReceivedChar(-1);
    uart_clear_intr_status(UART_NUM_EBUS, UART_BRK_DET_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_TXFIFO_EMPTY_INT_ST)) {
    // all good (Triggered when the amount of data in the transmit-FIFO is less than what tx_mem_cnttxfifo_cnt specifies)
    uart_clear_intr_status(UART_NUM_EBUS, UART_TXFIFO_EMPTY_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_RXFIFO_FULL_INT_ST)) {
    uart_clear_intr_status(UART_NUM_EBUS, UART_RXFIFO_FULL_INT_CLR);
  }
}

void setupQueues() {
  telegramHistoryQueue = xQueueCreateStatic(
      EBUS_TELEGRAM_HISTORY_QUEUE_SIZE,
      sizeof(Ebus::Telegram),
      &(telegramHistoryQueueStorage[0]),
      &telegramHistoryQueueBuffer);
  telegramCommandQueue = xQueueCreateStatic(
      EBUS_TELEGRAM_SEND_QUEUE_SIZE,
      sizeof(Ebus::Telegram),
      &(telegramCommandQueueStorage[0]),
      &telegramCommandQueueBuffer);
}


void setupEbusUart() {
  uart_config_t uart_config = {
      .baud_rate = 2400,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 2,
//     .source_clk = UART_SCLK_APB,
      .use_ref_tick = true,
  };
  ESP_ERROR_CHECK(uart_param_config(UART_NUM_EBUS, &uart_config));

  ESP_ERROR_CHECK(uart_set_pin(UART_NUM_EBUS, EBUS_UART_TX_PIN, EBUS_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_EBUS, 256, 0, 0, NULL, 0));

  uart_intr_config_t uart_intr = {
      .intr_enable_mask =
          UART_RXFIFO_FULL_INT_ENA_M,  //| UART_RXFIFO_TOUT_INT_ENA_M | UART_FRM_ERR_INT_ENA_M | UART_RXFIFO_OVF_INT_ENA_M | UART_BRK_DET_INT_ENA_M | UART_PARITY_ERR_INT_ENA_M,
      .rx_timeout_thresh = 10,
      .txfifo_empty_intr_thresh = 10,
      .rxfifo_full_thresh = 1,
  };
  ESP_ERROR_CHECK(uart_intr_config(UART_NUM_EBUS, &uart_intr));

  ESP_ERROR_CHECK(uart_isr_free(UART_NUM_EBUS));
  ESP_ERROR_CHECK(uart_isr_register(UART_NUM_EBUS, ebus_uart_intr_handle, NULL, ESP_INTR_FLAG_IRAM, NULL));
  ESP_ERROR_CHECK(uart_enable_rx_intr(UART_NUM_EBUS));
}

void ebusUartSend(const char *src, int16_t size) {
  uart_write_bytes(UART_NUM_EBUS, src, size);
}

void IRAM_ATTR ebusQueue(Ebus::Telegram telegram) {
  BaseType_t xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendToBackFromISR(telegramHistoryQueue, &telegram, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

bool IRAM_ATTR ebusDequeueCommand(void *const command) {
  BaseType_t xTaskWokenByReceive = pdFALSE;
  if (xQueueReceiveFromISR(telegramCommandQueue, command, &xTaskWokenByReceive)) {
    if (xTaskWokenByReceive) {
      portYIELD_FROM_ISR();
    }
    return true;
  }
  return false;
}

void setupEbus() {
  ebus.setUartSendFunction(ebusUartSend);
  ebus.setQueueHistoricFunction(ebusQueue);
  ebus.setDeueueCommandFunction(ebusDequeueCommand);
}

void processHistoricMessages(void *pvParameter) {
  Ebus::Telegram telegram;
  while (1) {
    if (xQueueReceive(telegramHistoryQueue, &telegram, portMAX_DELAY)) {
      handleMessage(telegram);
      //debugLogger(telegram);
      //tftLogger(telegram);
      taskYIELD();
    } else {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

void periodic(void *pvParameter) {
  Ebus::SendCommand getIdCommandSelf = Ebus::SendCommand(EBUS_MASTER_ADDRESS, EBUS_SLAVE_ADDRESS(EBUS_MASTER_ADDRESS), 0x07, 0x04, 0, NULL);
  Ebus::SendCommand getIdCommandHeater = Ebus::SendCommand(EBUS_MASTER_ADDRESS, EBUS_SLAVE_ADDRESS(EBUS_HEATER_MASTER_ADDRESS), 0x07, 0x04, 0, NULL);
  uint8_t getHwcWaterflowData[] = {0x0D, 0x55, 0x00};
  Ebus::SendCommand getHwcWaterflowCommand = Ebus::SendCommand(EBUS_MASTER_ADDRESS, EBUS_SLAVE_ADDRESS(EBUS_HEATER_MASTER_ADDRESS), 0xB5, 0x09, sizeof(getHwcWaterflowData), getHwcWaterflowData);
  //uint8_t getHwcDemandData[] = {0x0D, 0x58, 0x00};
  //Ebus::SendCommand getHwcDemandcommand = Ebus::SendCommand(EBUS_MASTER_ADDRESS, EBUS_SLAVE_ADDRESS(EBUS_HEATER_MASTER_ADDRESS), 0xB5, 0x09, sizeof(getHwcDemandData), getHwcData);
  uint8_t getFlameData[] = {0x0D, 0x05, 0x00};
  Ebus::SendCommand getFlameCommand = Ebus::SendCommand(EBUS_MASTER_ADDRESS, EBUS_SLAVE_ADDRESS(EBUS_HEATER_MASTER_ADDRESS), 0xB5, 0x09, sizeof(getFlameData), getFlameData);
  while (1) {
    xQueueSendToBack(telegramCommandQueue, &getIdCommandSelf, portMAX_DELAY);
    xQueueSendToBack(telegramCommandQueue, &getIdCommandHeater, portMAX_DELAY);
    xQueueSendToBack(telegramCommandQueue, &getHwcWaterflowCommand, portMAX_DELAY);
    //xQueueSendToBack(telegramCommandQueue, &getHwcDemandcommand, portMAX_DELAY);
    xQueueSendToBack(telegramCommandQueue, &getFlameCommand, portMAX_DELAY);
    printf("queued commands\n");
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

extern "C" {
void app_main();
}

void app_main() {
  printf("Setup\n");

  setupDisplay();
  setupQueues();
  setupEbusUart();
  setupEbus();

  xTaskCreate(&processHistoricMessages, "processHistoricMessages", 2048, NULL, 5, NULL);
  xTaskCreate(&periodic, "periodic", 2048, NULL, 5, NULL);
  xTaskCreate(&updateDisplay, "updateDisplay", 2048, NULL, 5, NULL);
  xTaskCreate(&setupWiFiAndKeepAlive, "setupWiFiAndKeepAlive", 4096, NULL, 3, NULL);
}

#else
// keep `pio run` happy
int main() {
  return 0;
}
#endif
