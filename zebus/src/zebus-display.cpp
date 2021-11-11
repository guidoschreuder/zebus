#include "zebus-display.h"

#include <TFT_eSPI.h>
#include <time.h>

#include "zebus-config.h"
#include "zebus-events.h"
#include "zebus-messages.h"
#include "zebus-system-info.h"
#include "zebus-state.h"

#define GET_BYTE(INT32, I) ((INT32 >> 8 * I) & 0XFF)

// variables
bool displayInit = false;

TFT_eSPI tft = TFT_eSPI(); // Invoke library
TFT_eSprite spriteShower = TFT_eSprite(&tft);
TFT_eSprite spriteHeater = TFT_eSprite(&tft);
uint16_t heaterWaveOffset = 0;

TFT_eSprite spriteWifiStrength = TFT_eSprite(&tft);
TFT_eSprite spriteEbusQueue = TFT_eSprite(&tft);

// 4-bit Palette colour table
uint16_t palette[16];

measurement_bool flame;
measurement_float flow;
uint8_t queue_size;

// upfront implementations
// for now this is just to keep the defines with the palette
void init4BitPalette() {
  palette[0]  = TFT_BLACK;
  palette[1]  = TFT_ORANGE;
  palette[2]  = TFT_DARKGREEN;
  palette[3]  = TFT_DARKCYAN;
  palette[4]  = TFT_MAROON;
  palette[5]  = TFT_PURPLE;
  palette[6]  = TFT_OLIVE;
  palette[7]  = TFT_DARKGREY;
  palette[8]  = TFT_ORANGE;
  palette[9]  = TFT_BLUE;
  palette[10] = TFT_GREEN;
  palette[11] = TFT_CYAN;
  palette[12] = TFT_RED;
  palette[13] = TFT_NAVY;
  palette[14] = TFT_YELLOW;
  palette[15] = TFT_WHITE;
}

#define EBUS_4BIT_BLACK 0
#define EBUS_4BIT_DARKCYAN 3
#define EBUS_4BIT_DARKGREY 7
#define EBUS_4BIT_ORANGE 8
#define EBUS_4BIT_GREEN 10
#define EBUS_4BIT_CYAN 11
#define EBUS_4BIT_RED 12
#define EBUS_4BIT_NAVY 13
#define EBUS_4BIT_YELLOW 14
#define EBUS_4BIT_WHITE 15

// prototypes
void display_handle_flame(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void display_handle_flow(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void display_handle_queue_size(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void setupDisplay();
void disableDisplay();

// prototype: init
void initSprites();
void initSprite(TFT_eSprite &sprite, int16_t width, int16_t height);

// prototypes: update
void updateDisplay();

// prototypes: draw sprites
void drawSpriteShower(int32_t x, int32_t y, float flow);
void drawSpriteHeater(int32_t x, int32_t y, bool on);
void drawSpriteWifiStrength(int32_t x, int32_t y, int32_t rssi);
void drawSpriteEbusQueue(int32_t x, int32_t y, uint8_t queue_size);

//prototypes: helpers
size_t print_time(struct tm * timeinfo, const char * format);
void print_ip_addr();

// public functions
void displayTask(void *pvParameter) {
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_FLAME, display_handle_flame, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_FLOW, display_handle_flow, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_FLOW, display_handle_queue_size, NULL));
  EventGroupHandle_t event_group = (EventGroupHandle_t) pvParameter;
  for (;;) {
    EventBits_t uxBits = xEventGroupGetBits(event_group);
    if (uxBits & DISPLAY_ENABLED) {
      setupDisplay();
      updateDisplay();
    } else if (displayInit) {
      disableDisplay();
      xEventGroupSetBits(event_group, DISPLAY_DISABLED);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void display_handle_flame(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  flame = *((measurement_bool*) event_data);
}

void display_handle_flow(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  flow = *((measurement_float*) event_data);
}

void display_handle_queue_size(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  queue_size = *((uint8_t*) event_data);
}

// implementations
void setupDisplay() {
  if (displayInit) {
    return;
  }
  ESP_LOGI(ZEBUS_LOG_TAG, "Setup display");
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Set "cursor" at top left corner of display (0,0) and select font 1
  tft.setCursor(0, 0, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //  tft.println("Initialised default\n");

  initSprites();

  displayInit = true;
}

void disableDisplay() {
  ESP_LOGI(ZEBUS_LOG_TAG, "Disable display");

  // TODO: this should be replaced with actual shutdown when display is controlled through a mosfet
  tft.fillScreen(TFT_BLACK);

  displayInit = false;
}

void initSprites() {
  init4BitPalette();
  initSprite(spriteShower, 30, 30);
  initSprite(spriteHeater, 30, 30);
  initSprite(spriteWifiStrength, 16, 16);
  initSprite(spriteEbusQueue, 16, 16);
}

void initSprite(TFT_eSprite &sprite, int16_t width, int16_t height) {
  sprite.setColorDepth(4);
  sprite.createSprite(width, height);
  sprite.createPalette(palette);
  sprite.fillSprite(EBUS_4BIT_BLACK);
}

void updateDisplay() {
  tft.setCursor(0, 0, 1);
  if (system_info->wifi.config_ap.active) {
    tft.printf("AP Name    : %s\n", system_info->wifi.config_ap.ap_name);
    tft.printf("AP Password: %s\n", system_info->wifi.config_ap.ap_password);
  } else {
    tft.println("                                  ");
    tft.println("                                  ");
  }
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    print_time(&timeinfo, "%c\n");
    tft.println();
  }

  print_ip_addr();
  tft.println();
  tft.printf("Command Queue size: %d             \n", queue_size);

  tft.setCursor(0, 195, 1);
  tft.printf("Self  : %s, sw: %s, hw: %s\n", system_info->ebus.self_id.device, system_info->ebus.self_id.sw_version, system_info->ebus.self_id.hw_version);
  tft.printf("Heater: %s, sw: %s, hw: %s\n", system_info->ebus.heater_id.device, system_info->ebus.heater_id.sw_version, system_info->ebus.heater_id.hw_version);
  tft.printf("Flame : %s       \n", flame.valid() ? (flame.value ? "ON" : "OFF") : "UNKNOWN");
  tft.printf("Flow  : %.2f     \n", flow.valid() ? flow.value : -1 );

  drawSpriteHeater(280, 10, true);
  drawSpriteHeater(240, 10, false);
  drawSpriteShower(280, 50, flow.valid() ? flow.value : 0);
  drawSpriteWifiStrength(280, 90, system_info->wifi.rssi);
  drawSpriteEbusQueue(240, 90, queue_size);
}

void drawSpriteShower(int32_t x, int32_t y, float flow) {
  // scale flow to 8 bits
  int8_t scaledFlow = flow >= EBUS_HEATER_MAX_FLOW ? 255 : (int8_t) (flow * 255 / EBUS_HEATER_MAX_FLOW);

  // remove previous water
  spriteShower.fillRect(10, 12, 20, 18, EBUS_4BIT_BLACK);

  spriteShower.fillRoundRect(16, 0, 8, 12, 1, scaledFlow ? EBUS_4BIT_RED : EBUS_4BIT_BLACK);
  spriteShower.fillRoundRect(0, 2, 14, 4, 1, scaledFlow ? EBUS_4BIT_RED : EBUS_4BIT_BLACK);
  if (!scaledFlow) {
    spriteShower.drawRoundRect(16, 0, 8, 12, 1, EBUS_4BIT_WHITE);
    spriteShower.drawRoundRect(0, 2, 14, 4, 1, EBUS_4BIT_WHITE);
  } else {
    for (uint8_t i = 0; i < 2 + (scaledFlow / 2); i++) {
      uint16_t w_y = random(16);
      uint16_t w_x = random((scaledFlow * w_y  / 512) + 3);
      w_x = 20 + (random(2) ? w_x : -w_x);
      uint8_t c = EBUS_4BIT_CYAN;
      uint8_t rnd = random(256);
      if (rnd < 32) {
        c = EBUS_4BIT_DARKCYAN;
      } else if (rnd < 64) {
        c = EBUS_4BIT_NAVY;
      }
      spriteShower.drawLine(w_x, w_y + 12, w_x, w_y + 14, c);
    }
  }
  spriteShower.pushSprite(x, y);
}

void drawSpriteHeater(int32_t x, int32_t y, bool on) {
  const uint16_t WAVE_Y_OFFSET = 6;
  spriteHeater.fillRect(0, 0, 30, WAVE_Y_OFFSET + 1, EBUS_4BIT_BLACK);
  for (int i = 0; i < 4; i++) {
    spriteHeater.drawRoundRect(8 * i, WAVE_Y_OFFSET, 5, 24, 1, on ? EBUS_4BIT_ORANGE : EBUS_4BIT_WHITE);
    if (i < 3) {
      spriteHeater.drawFastHLine(8 * i + 5, WAVE_Y_OFFSET + 5, 2, on ? EBUS_4BIT_ORANGE : EBUS_4BIT_WHITE);
      spriteHeater.drawFastHLine(8 * i + 5, WAVE_Y_OFFSET + 20, 2, on ? EBUS_4BIT_ORANGE : EBUS_4BIT_WHITE);
    }
  }
  if (on) {
    uint8_t wave_y_prev = WAVE_Y_OFFSET - 3, wave_width = 29;
    for (int8_t wave_x = -1; wave_x < wave_width; wave_x++) {
      uint8_t wave_y = WAVE_Y_OFFSET - 4 + sinf((wave_x + heaterWaveOffset)/ 2.0) * 2;
      spriteHeater.drawRect(wave_x - 1, wave_y_prev, 2, 2, EBUS_4BIT_RED);
      wave_y_prev = wave_y;
    }
    heaterWaveOffset++;
  }
  spriteHeater.pushSprite(x, y);
}

void drawSpriteWifiStrength(int32_t x, int32_t y, int32_t rssi) {
  uint8_t lvl = 0;
  if (rssi >= -50) {
    lvl = 4;
  } else if (rssi >= -60) {
    lvl = 3;
  } else if (rssi >= -70) {
    lvl = 2;
  } else if (rssi >= -80) {
    lvl = 1;
  }
  // draw red cross when no signal, overwrite in black otherwise
  uint32_t c = rssi == WIFI_NO_SIGNAL ? EBUS_4BIT_RED : EBUS_4BIT_BLACK;
  spriteWifiStrength.drawLine(0, 0, 8, 8, c);
  spriteWifiStrength.drawLine(1, 0, 9, 8, c);
  spriteWifiStrength.drawLine(8, 0, 0, 8, c);
  spriteWifiStrength.drawLine(9, 0, 1, 8, c);
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t h = 3 * (i + 1);
    spriteWifiStrength.fillRect(3 * i + 4, 16 - h, 2, h, lvl > i ? EBUS_4BIT_GREEN : EBUS_4BIT_DARKGREY);
  }
  spriteWifiStrength.pushSprite(x, y);
}

void drawSpriteEbusQueue(int32_t x, int32_t y, uint8_t queue_size) {
  uint8_t cutoff = queue_size * 16 / EBUS_TELEGRAM_SEND_QUEUE_SIZE;
  for (uint8_t i = 0; i < 16; i++) {
    uint32_t color = EBUS_4BIT_GREEN;
    if (i > cutoff) {
      color = EBUS_4BIT_BLACK;
    } else if (i >= 12) {
      color = EBUS_4BIT_RED;
    } else if (i >= 8) {
      color = EBUS_4BIT_ORANGE;
    } else if (i >= 4) {
      color = EBUS_4BIT_YELLOW;
    }
    spriteEbusQueue.drawFastVLine(i, 7, 8, color);
  }
  spriteEbusQueue.pushSprite(x, y);
}

size_t print_time(struct tm * timeinfo, const char * format) {
    char buf[64];
    size_t written = strftime(buf, sizeof(buf), format, timeinfo);
    if (written == 0) {
      return written;
    }
    return tft.printf(buf);
}


void print_ip_addr() {
  uint32_t ip = system_info->wifi.ip_addr;
  tft.printf("%d.%d.%d.%d", GET_BYTE(ip, 0), GET_BYTE(ip, 1), GET_BYTE(ip, 2), GET_BYTE(ip, 3));
}
