#include "zebus-ebus.h"

#include <driver/uart.h>
#include <freertos/queue.h>

#include "Ebus.h"
#include "zebus-config.h"
#include "zebus-messages.h"
#include "zebus-system-info.h"

#define CHECK_INT_STATUS(ST, MASK) (((ST) & (MASK)) == (MASK))

bool ebusInit = false;

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
void initEbus();
void enqueueEbusCommand(const Ebus::SendCommand &);
void ebusPoll();
void setupQueues();
void setupEbusUart();
void ebusUartSend(const char *src, int16_t size);
void ebusQueue(Ebus::Telegram &telegram);
bool ebusDequeueCommand(void *const command);
void processReceivedMessages(void *pvParameter);
void processReceivedEbusBytes(void *pvParameter);
static void ebus_uart_intr_handle(void *arg);

// public functions
void ebusTask(void *pvParameter) {
  while(1) {
    initEbus();
    ebusPoll();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// implementations
void initEbus() {
  if (ebusInit) {
    return;
  }
  setupQueues();

  ebus.setUartSendFunction(ebusUartSend);
  ebus.setQueueReceivedTelegramFunction(ebusQueue);
  ebus.setDeueueCommandFunction(ebusDequeueCommand);
  ebus.addSendResponseHandler(sendIdentificationResponse);

  setupEbusUart();

  xTaskCreate(&processReceivedMessages, "processReceivedMessages", 2560, NULL, 5, NULL);
  xTaskCreate(&processReceivedEbusBytes, "processReceivedEbusBytes", 2048, NULL, 1, NULL);

  ebusInit = true;
}

// TODO: this is all very much test code for now
void ebusPoll() {
  enqueueEbusCommand(createCommand(EBUS_MASTER_ADDRESS, CMD_IDENTIFICATION));
  enqueueEbusCommand(createHeaterCommand(CMD_IDENTIFICATION));
  enqueueEbusCommand(createHeaterReadConfigCommand(DEVICE_CONFIG_HWC_WATERFLOW));
  enqueueEbusCommand(createHeaterReadConfigCommand(DEVICE_CONFIG_FLAME));
  enqueueEbusCommand(createHeaterReadConfigCommand(DEVICE_CONFIG_FLOW_TEMP));
  enqueueEbusCommand(createHeaterReadConfigCommand(DEVICE_CONFIG_RETURN_TEMP));
  enqueueEbusCommand(createHeaterReadConfigCommand(DEVICE_CONFIG_EBUS_CONTROL));

  // TODO: OK, this does seem to work but we do not have a proper way to set or control the room temparature yet
  // uint8_t setModeData[] = {0x00, 0x00, 0x5E, 0x78, 0xFF, 0xFF, 0x00, 0xFF, 0x00};
  // enqueueEbusCommand(createHeaterCommand(CMD_DATA_TO_BURNER, sizeof(setModeData), setModeData));

  //enqueueEbusCommand(createHeaterReadConfigCommand(DEVICE_CONFIG_HWC_DEMAND));
}

void enqueueEbusCommand(const Ebus::SendCommand &command) {
  xQueueSendToBack(telegramCommandQueue, &command, portMAX_DELAY);
  system_info->ebus.queue_size = uxQueueMessagesWaiting(telegramCommandQueue);
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
  receivedByteQueue = xQueueCreateStatic(
      EBUS_TELEGRAM_RECEIVE_BYTE_QUEUE_SIZE,
      sizeof(uint8_t),
      &(receivedByteQueueStorage[0]),
      &receivedByteQueueBuffer);
}

void setupEbusUart() {

  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&mux);

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

  portEXIT_CRITICAL(&mux);
}

void ebusUartSend(const char *src, int16_t size) {
  uart_write_bytes(UART_NUM_EBUS, src, size);
}

void ebusQueue(Ebus::Telegram &telegram) {
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
    system_info->ebus.queue_size = uxQueueMessagesWaiting(telegramCommandQueue);
    if (xTaskWokenByReceive) {
      portYIELD_FROM_ISR();
    }
    return true;
  }
  return false;
}

void processReceivedMessages(void *pvParameter) {
  Ebus::Telegram telegram;
  while (1) {
    if (xQueueReceive(telegramHistoryQueue, &telegram, pdMS_TO_TICKS(1000))) {
      handleMessage(telegram);
      //ESP_LOGD(ZEBUS_LOG_TAG, "Task: %s, Stack Highwater Mark: %d", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
      taskYIELD();
    }
  }
}

void processReceivedEbusBytes(void *pvParameter) {
  while (1) {
    uint8_t receivedByte;
    if (xQueueReceive(receivedByteQueue, &receivedByte, pdMS_TO_TICKS(1000))) {
      ebus.processReceivedChar(receivedByte);
      taskYIELD();
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
