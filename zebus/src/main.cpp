
#ifndef UNIT_TEST

#include "main.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sdkconfig.h"
#include "zebus-display.h"
#include "zebus-ebus.h"
#include "zebus-messages.h"
#include "zebus-system-info.h"
#include "zebus-wifi.h"

struct system_info_t* system_info = new system_info_t();

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

    ESP_LOGD(ZEBUS_LOG_TAG, "queued commands");
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

extern "C" {
void app_main();
}

void app_main() {
  ESP_LOGI(ZEBUS_LOG_TAG, "Setup %s", ZEBUS_APPNAME);

  xTaskCreate(&displayTask, "displayTask", 2048, NULL, 5, NULL);
  xTaskCreate(&wiFiTask, "wiFiTask", 8192, NULL, 3, NULL);

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
