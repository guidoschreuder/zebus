#include "zebus-ebus.h"

#include "Ebus.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "zebus-config.h"
#include "zebus-messages.h"
#include "zebus-system-info.h"

#define CHECK_INT_STATUS(ST, MASK) (((ST) & (MASK)) == (MASK))

// queues
QueueHandle_t telegramHistoryQueue;
StaticQueue_t telegramHistoryQueueBuffer;
uint8_t telegramHistoryQueueStorage[EBUS_TELEGRAM_HISTORY_QUEUE_SIZE * sizeof(Ebus::Telegram)];
QueueHandle_t telegramCommandQueue;
StaticQueue_t telegramCommandQueueBuffer;
uint8_t telegramCommandQueueStorage[EBUS_TELEGRAM_SEND_QUEUE_SIZE * sizeof(Ebus::Telegram)];
QueueHandle_t receivedByteQueue;
StaticQueue_t receivedByteQueueBuffer;
uint8_t receivedByteQueueStorage[EBUS_TELEGRAM_RECEIVE_BYTE_QUEUE_SIZE];

ebus_config_t ebus_config = ebus_config_t {
  .master_address = EBUS_MASTER_ADDRESS,
  .max_tries = EBUS_MAX_TRIES,
  .max_lock_counter = EBUS_MAX_LOCK_COUNTER,
};

Ebus::Ebus ebus = Ebus::Ebus(ebus_config);

// prototypes
void setupQueues();
void setupEbusUart();
void ebusUartSend(const char *src, int16_t size);
void ebusQueue(Ebus::Telegram telegram);
bool ebusDequeueCommand(void *const command);
void processHistoricMessages(void *pvParameter);
void processReceivedEbusBytes(void *pvParameter);
static void ebus_uart_intr_handle(void *arg);

// public methods
void setupEbus() {
  setupQueues();
  setupEbusUart();

  ebus.setUartSendFunction(ebusUartSend);
  ebus.setQueueHistoricFunction(ebusQueue);
  ebus.setDeueueCommandFunction(ebusDequeueCommand);
  ebus.addSendResponseHandler(sendIdentificationResponse);

  xTaskCreate(&processHistoricMessages, "processHistoricMessages", 2048, NULL, 5, NULL);
  xTaskCreate(&processReceivedEbusBytes, "processReceivedEbusBytes", 2048, NULL, 1, NULL);
}

void enqueueEbusCommand(const void * const itemToQueue) {
    xQueueSendToBack(telegramCommandQueue, itemToQueue, portMAX_DELAY);
    system_info->ebus.queue_size = uxQueueMessagesWaiting(telegramCommandQueue);
}

// implementations
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
  receivedByteQueue = xQueueCreateStatic(
      EBUS_TELEGRAM_RECEIVE_BYTE_QUEUE_SIZE,
      sizeof(uint8_t),
      &(receivedByteQueueStorage[0]),
      &receivedByteQueueBuffer);
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
  ESP_ERROR_CHECK(uart_isr_register(UART_NUM_EBUS, ebus_uart_intr_handle, NULL, 0 /*ESP_INTR_FLAG_IRAM*/, NULL));
  ESP_ERROR_CHECK(uart_enable_rx_intr(UART_NUM_EBUS));
}


void ebusUartSend(const char *src, int16_t size) {
  uart_write_bytes(UART_NUM_EBUS, src, size);
}

void ebusQueue(Ebus::Telegram telegram) {
  BaseType_t xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendToBackFromISR(telegramHistoryQueue, &telegram, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

bool ebusDequeueCommand(void *const command) {
  BaseType_t xTaskWokenByReceive = pdFALSE;
  if (xQueueReceiveFromISR(telegramCommandQueue, command, &xTaskWokenByReceive)) {
    if (xTaskWokenByReceive) {
      portYIELD_FROM_ISR();
    }
    return true;
  }
  return false;
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

void processReceivedEbusBytes(void *pvParameter) {
  while (1) {
    uint8_t receivedByte;
    if (xQueueReceive(receivedByteQueue, &receivedByte, portMAX_DELAY)) {
      ebus.processReceivedChar(receivedByte);
      taskYIELD();
    } else {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

void IRAM_ATTR ebus_uart_intr_handle(void *arg) {
  uint16_t status = UART_EBUS.int_st.val;  // read UART interrupt Status
  if (status == 0) {
    return;
  }

  uint16_t rx_fifo_len = UART_EBUS.status.rxfifo_cnt;  // read number of bytes in UART buffer
  BaseType_t xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  while (rx_fifo_len) {
    uint8_t cr = UART_EBUS.fifo.rw_byte;
    xQueueSendToBackFromISR(receivedByteQueue, &cr, &xHigherPriorityTaskWoken);
    rx_fifo_len--;
  }

  if (CHECK_INT_STATUS(status, UART_SW_XON_INT_ST)) {
    uart_clear_intr_status(UART_NUM_EBUS, UART_SW_XON_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_FRM_ERR_INT_ST)) {
    uart_clear_intr_status(UART_NUM_EBUS, UART_FRM_ERR_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_BRK_DET_INT_ST)) {
    uart_clear_intr_status(UART_NUM_EBUS, UART_BRK_DET_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_TXFIFO_EMPTY_INT_ST)) {
    uart_clear_intr_status(UART_NUM_EBUS, UART_TXFIFO_EMPTY_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_RXFIFO_FULL_INT_ST)) {
    uart_clear_intr_status(UART_NUM_EBUS, UART_RXFIFO_FULL_INT_CLR);
  }

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}
