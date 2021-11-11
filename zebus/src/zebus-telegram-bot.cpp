#include "zebus-telegram-bot.h"

#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "zebus-config.h"
#include "zebus-events.h"
#include "zebus-secrets.h"
#include "zebus-system-info.h"
#include "zebus-time.h"

WiFiClientSecure client;
UniversalTelegramBot telegramBot(ZEBUS_TELEGRAM_TOKEN, client);

bool telegramInit = false;
uint64_t bot_lasttime; // last time messages' scan has been done

measurement_temperature_sensor room;
measurement_temperature_sensor outdoor;

void telegram_handle_sensor(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  measurement_temperature_sensor data = *((measurement_temperature_sensor*) event_data);
  if (strcmp(data.value.location, "room") == 0) {
    room = data;
  } else if (strcmp(data.value.location, "outdoor") == 0) {
    outdoor = data;
  }
}

void setupTelegram() {
  if (telegramInit) {
    return;
  }
  ESP_LOGD(ZEBUS_LOG_TAG, "Setup Telegram Bot");
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_SENSOR, telegram_handle_sensor, NULL));

  telegramInit = true;
}

void add_sensor_to_reply(String &reply, measurement_temperature_sensor &sensor) {
  if (sensor.valid()) {
    reply += "\nSensor " + String(sensor.value.location) + ":\n  Temperature: ";
    reply += sensor.value.temperatureC;
    reply += "\n  Voltage: ";
    reply += sensor.value.supplyVoltage;
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    if (telegramBot.messages[i].chat_id != ZEBUS_TELEGRAM_CLIENT_ID) {
      telegramBot.sendMessage(telegramBot.messages[i].chat_id, "Unauthorized user");
      continue;
    }
    if (telegramBot.messages[i].text == "/status") {
      String reply;
      reply = "Heater: ";
      reply += system_info->ebus.heater_id.device;
      reply += " (SW: ";
      reply += system_info->ebus.heater_id.sw_version;
      reply += ", HW: ";
      reply += system_info->ebus.heater_id.hw_version;
      reply += ")";

      add_sensor_to_reply(reply, room);
      add_sensor_to_reply(reply, outdoor);

      telegramBot.sendMessage(telegramBot.messages[i].chat_id, reply);
    }
  }
}

void handleTelegramMessages() {
  setupTelegram();

  if (interval_expired(&bot_lasttime, ZEBUS_TELEGRAM_MTBS_MS)) {
    ESP_LOGV(ZEBUS_LOG_TAG, "telegram poll");

    int numNewMessages = telegramBot.getUpdates(telegramBot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = telegramBot.getUpdates(telegramBot.last_message_received + 1);
    }
  }
}
