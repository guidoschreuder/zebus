#ifndef _ZEBUS_CONFIG_H
#define _ZEBUS_CONFIG_H

// application configuration
#define ZEBUS_APPNAME "Zebus"

#define ZEBUS_LOG_LEVEL ESP_LOG_DEBUG

// Network configuration
#define ZEBUS_WIFI_CONFIG_AP_PASSWORD_LENGTH 8
#define ZEBUS_WIFI_HOSTNAME "zebus"

#define ZEBUS_NTP_SERVER "1.be.pool.ntp.org"
#define ZEBUS_NTP_GMT_OFFSET_SEC 3600
#define ZEBUS_NTP_GMT_DST_OFFSET_SEC 3600
#define ZEBUS_NTP_REFRESH_INTERVAL_SEC (5 * 60)

// MQTT config
#define ZEBUS_MQTT_HOST "nas.home.arpa"
#define ZEBUS_MQTT_TRANSPORT MQTT_TRANSPORT_OVER_TCP
#define ZEBUS_MQTT_UPDATE_INTERVAL_MS (10 * 1000)

// Sensors
#define ZEBUS_SENSOR_INTERVAL_MS 10000

// eBUS configuration
#define EBUS_UART_NUM 1
#define EBUS_UART_RX_PIN 32
#define EBUS_UART_TX_PIN 33

#define EBUS_MASTER_ADDRESS 0x00
#define EBUS_TELEGRAM_HISTORY_QUEUE_SIZE 50
#define EBUS_TELEGRAM_SEND_QUEUE_SIZE 20
#define EBUS_TELEGRAM_RECEIVE_BYTE_QUEUE_SIZE 1024
#define EBUS_MAX_TRIES 2
#define EBUS_MAX_LOCK_COUNTER 4

#define EBUS_HEATER_MASTER_ADDRESS 0x03
#define EBUS_HEATER_MAX_FLOW 12.0

#define EBUS_DEVICE_VENDOR_ID 0xDD
#define EBUS_DEVICE_NAME ZEBUS_APPNAME
#define EBUS_DEVICE_SW_VERSION 0x0102
#define EBUS_DEVICE_HW_VERSION 0x0304

// Telegram configuration
#define ZEBUS_TELEGRAM_MTBS_MS 500

#endif
