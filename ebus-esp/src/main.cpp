
#if defined(ARDUINO) && !defined(UNIT_TEST)

#include "Arduino.h"

#include "main.h"
#include "ebus.h"
#include "driver/uart.h"

#define CHECK_INT_STATUS(ST, MASK) (((ST) & (MASK)) == (MASK))

bool isr_called = false;

static void IRAM_ATTR ebus_uart_intr_handle(void *arg) {
  isr_called = true;

  uint16_t status = UART_EBUS.int_st.val;  // read UART interrupt Status
  if (status == 0) {
    return;
  }

  uint16_t rx_fifo_len = UART_EBUS.status.rxfifo_cnt;  // read number of bytes in UART buffer
  while (rx_fifo_len) {
    process_received(UART_EBUS.fifo.rw_byte);
    rx_fifo_len--;
  }

  if (status != 256) {
    Serial.print("Status:");
    Serial.println(status);
  }
  if (CHECK_INT_STATUS(status, UART_SW_XON_INT_ST)) {
    uart_clear_intr_status(UART_NUM_EBUS, UART_SW_XON_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_FRM_ERR_INT_ST)) {
    // process error (Triggered when the receiver detects a data frame error)
    uart_clear_intr_status(UART_NUM_EBUS, UART_FRM_ERR_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_BRK_DET_INT_ST)) {
    // process error (Triggered when the receiver detects a 0 level after the STOP bit)
    uart_clear_intr_status(UART_NUM_EBUS, UART_BRK_DET_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_TXFIFO_EMPTY_INT_ST)) {
    // all good (Triggered when the amount of data in the transmit-FIFO is less than what tx_mem_cnttxfifo_cnt specifies)
    uart_clear_intr_status(UART_NUM_EBUS, UART_TXFIFO_EMPTY_INT_CLR);
  } else if (CHECK_INT_STATUS(status, UART_RXFIFO_FULL_INT_ST)) {
    uart_clear_intr_status(UART_NUM_EBUS, UART_RXFIFO_FULL_INT_CLR);
  }

}

void setupEbusUart() {
  uart_config_t uart_config = {
      .baud_rate = 2400,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
  ESP_ERROR_CHECK(uart_param_config(UART_NUM_EBUS, &uart_config));

  uart_intr_config_t uart_intr = {
      .intr_enable_mask =
          UART_RXFIFO_FULL_INT_ENA_M | UART_RXFIFO_TOUT_INT_ENA_M | UART_FRM_ERR_INT_ENA_M | UART_RXFIFO_OVF_INT_ENA_M | UART_BRK_DET_INT_ENA_M | UART_PARITY_ERR_INT_ENA_M,
      .rx_timeout_thresh = 10,
      .txfifo_empty_intr_thresh = 10,
      .rxfifo_full_thresh = 1};
  ESP_ERROR_CHECK(uart_intr_config(UART_NUM_EBUS, &uart_intr));

  ESP_ERROR_CHECK(uart_set_pin(UART_NUM_EBUS, UART_PIN_NO_CHANGE, EBUS_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_EBUS, 256, 0, 0, NULL, 0));

  ESP_ERROR_CHECK(uart_isr_free(UART_NUM_EBUS));
  ESP_ERROR_CHECK(uart_isr_register(UART_NUM_EBUS, ebus_uart_intr_handle, NULL, ESP_INTR_FLAG_IRAM, NULL));
  ESP_ERROR_CHECK(uart_enable_rx_intr(UART_NUM_EBUS));
}

void show(void *pvParameter) {
  int32_t i;
  while (1) {
    Serial.print(g_serialBuffer_pos);
    Serial.print(": ");
    for (i = 0; i < EBUS_SERIAL_BUFFER_SIZE; i++) {
      if (i == g_serialBuffer_pos) {
        Serial.print('>');
      }
      if (g_serialBuffer[i] < 0X10) {
        Serial.print(0);
      }
      Serial.print(g_serialBuffer[i], HEX);
    }
    Serial.println();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }
  Serial.println("Setup");

  setupEbusUart();
  xTaskCreate(&show, "show", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
}

void loop() {
  //
}

#else
// keep `pio run` happy
int main() {
  return 0;
}
#endif
