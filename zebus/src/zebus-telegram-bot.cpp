#include "zebus-telegram-bot.h"

#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "zebus-config.h"
#include "zebus-secrets.h"
#include "zebus-system-info.h"
#include "zebus-time.h"

WiFiClientSecure client;
UniversalTelegramBot telegramBot(ZEBUS_TELEGRAM_TOKEN, client);

bool telegramInit = false;
uint64_t bot_lasttime; // last time messages' scan has been done

void setupTelegram() {
  if (telegramInit) {
    return;
  }
  ESP_LOGD(ZEBUS_LOG_TAG, "Setup Telegram Bot");
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  telegramInit = true;
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

      for (uint8_t i = 0; i < system_info->num_sensors; i++) {
        if (system_info->sensors[i].valid()) {
          reply += "\nSensor " + String(system_info->sensors[i].value.location) + ":\n  Temperature: ";
          reply += system_info->sensors[i].value.temperatureC;
          reply += "\n  Voltage: ";
          reply += system_info->sensors[i].value.supplyVoltage;
        }
      }

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
