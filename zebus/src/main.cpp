
#ifndef UNIT_TEST

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include "sdkconfig.h"
#include "zebus-config.h"
#include "zebus-display.h"
#include "zebus-ebus.h"
#include "zebus-messages.h"
#include "zebus-system-info.h"
#include "zebus-wifi.h"

struct system_info_t* system_info = new system_info_t();

void debugLogger(Ebus::Telegram telegram) {
  printf(
    "===========\nstate: %d\nQQ: %02X\tZZ: %02X\tPB: %02X\tSB: %02X\nreq(size: %d, CRC: %02x): ",  //
    telegram.getState(),
    telegram.getQQ(),
    telegram.getZZ(),
    telegram.getPB(),
    telegram.getSB(),
    telegram.getNN(),
    telegram.getRequestCRC());
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
    enqueueEbusCommand(&getIdCommandSelf);
    enqueueEbusCommand(&getIdCommandHeater);
    enqueueEbusCommand(&getHwcWaterflowCommand);
    //enqueueEbusCommand(&getHwcDemandcommand);
    enqueueEbusCommand(&getFlameCommand);

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

  // CRITICAL: WiFi will fail to startup without this
  // see: https://github.com/espressif/arduino-esp32/issues/761
  nvs_flash_init();
  vTaskDelay(pdMS_TO_TICKS(500));

  xTaskCreate(&updateDisplay, "updateDisplay", 2048, NULL, 5, NULL);
  xTaskCreate(&wiFiLoop, "setupWiFiAndKeepAlive", 4096, NULL, 3, NULL);

  vTaskDelay(pdMS_TO_TICKS(500));
  setupEbus();

  xTaskCreate(&periodic, "periodic", 2048, NULL, 5, NULL);

}

#else
// keep `pio run` happy
int main() {
  return 0;
}
#endif
