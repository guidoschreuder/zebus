#include "zebus-sprites.h"

#include "zebus-config.h"
#include "zebus-system-info.h"

bool sprite_init_done;

TFT_eSprite *spriteShower;
TFT_eSprite *spriteHeater;
uint16_t heaterWaveOffset = 0;

TFT_eSprite *spriteWifiStrength;
TFT_eSprite *spriteEbusQueue;

// prototype
TFT_eSprite *init_sprite(TFT_eSPI *tft, int16_t width, int16_t height);

// 4-bit Palette colour table
uint16_t palette[16];

// upfront implementations
// for now this is just to keep the defines with the palette
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

void init4BitPalette() {
  palette[EBUS_4BIT_BLACK]      = TFT_BLACK;
  palette[1]                    = TFT_ORANGE;
  palette[2]                    = TFT_DARKGREEN;
  palette[EBUS_4BIT_DARKCYAN]   = TFT_DARKCYAN;
  palette[4]                    = TFT_MAROON;
  palette[5]                    = TFT_PURPLE;
  palette[6]                    = TFT_OLIVE;
  palette[EBUS_4BIT_DARKGREY]   = TFT_DARKGREY;
  palette[EBUS_4BIT_ORANGE]     = TFT_ORANGE;
  palette[9]                    = TFT_BLUE;
  palette[EBUS_4BIT_GREEN]      = TFT_GREEN;
  palette[EBUS_4BIT_CYAN]       = TFT_CYAN;
  palette[EBUS_4BIT_RED]        = TFT_RED;
  palette[EBUS_4BIT_NAVY]       = TFT_NAVY;
  palette[EBUS_4BIT_YELLOW]     = TFT_YELLOW;
  palette[EBUS_4BIT_WHITE]      = TFT_WHITE;
}


void init_sprites(TFT_eSPI *tft) {
  if (sprite_init_done) {
    return;
  }
  sprite_init_done = true;
  init4BitPalette();
  spriteShower = init_sprite(tft, 30, 30);
  spriteHeater = init_sprite(tft, 30, 30);
  spriteWifiStrength = init_sprite(tft, 16, 16);
  spriteEbusQueue = init_sprite(tft, 16, 16);
}

TFT_eSprite *init_sprite(TFT_eSPI *tft, int16_t width, int16_t height) {
  TFT_eSprite *sprite = new TFT_eSprite(tft);
  sprite->setColorDepth(4);
  sprite->createSprite(width, height);
  sprite->createPalette(palette);
  sprite->fillSprite(EBUS_4BIT_BLACK);
  return sprite;
}

void draw_sprite_shower(int32_t x, int32_t y, float flow) {
  // scale flow to 8 bits
  int8_t scaledFlow = flow >= EBUS_HEATER_MAX_FLOW ? 255 : (int8_t) (flow * 255 / EBUS_HEATER_MAX_FLOW);

  // remove previous water
  spriteShower->fillRect(10, 12, 20, 18, EBUS_4BIT_BLACK);

  spriteShower->fillRoundRect(16, 0, 8, 12, 1, scaledFlow ? EBUS_4BIT_RED : EBUS_4BIT_BLACK);
  spriteShower->fillRoundRect(0, 2, 14, 4, 1, scaledFlow ? EBUS_4BIT_RED : EBUS_4BIT_BLACK);
  if (!scaledFlow) {
    spriteShower->drawRoundRect(16, 0, 8, 12, 1, EBUS_4BIT_WHITE);
    spriteShower->drawRoundRect(0, 2, 14, 4, 1, EBUS_4BIT_WHITE);
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
      spriteShower->drawLine(w_x, w_y + 12, w_x, w_y + 14, c);
    }
  }
  spriteShower->pushSprite(x, y);
}

void draw_sprite_heater(int32_t x, int32_t y, bool on) {
  const uint16_t WAVE_Y_OFFSET = 6;
  spriteHeater->fillRect(0, 0, 30, WAVE_Y_OFFSET + 1, EBUS_4BIT_BLACK);
  for (int i = 0; i < 4; i++) {
    spriteHeater->drawRoundRect(8 * i, WAVE_Y_OFFSET, 5, 24, 1, on ? EBUS_4BIT_ORANGE : EBUS_4BIT_WHITE);
    if (i < 3) {
      spriteHeater->drawFastHLine(8 * i + 5, WAVE_Y_OFFSET + 5, 2, on ? EBUS_4BIT_ORANGE : EBUS_4BIT_WHITE);
      spriteHeater->drawFastHLine(8 * i + 5, WAVE_Y_OFFSET + 20, 2, on ? EBUS_4BIT_ORANGE : EBUS_4BIT_WHITE);
    }
  }
  if (on) {
    uint8_t wave_y_prev = WAVE_Y_OFFSET - 3, wave_width = 29;
    for (int8_t wave_x = -1; wave_x < wave_width; wave_x++) {
      uint8_t wave_y = WAVE_Y_OFFSET - 4 + sinf((wave_x + heaterWaveOffset)/ 2.0) * 2;
      spriteHeater->drawRect(wave_x - 1, wave_y_prev, 2, 2, EBUS_4BIT_RED);
      wave_y_prev = wave_y;
    }
    heaterWaveOffset++;
  }
  spriteHeater->pushSprite(x, y);
}

void draw_sprite_wifi_strength(int32_t x, int32_t y, int32_t rssi) {
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
  spriteWifiStrength->drawLine(0, 0, 8, 8, c);
  spriteWifiStrength->drawLine(1, 0, 9, 8, c);
  spriteWifiStrength->drawLine(8, 0, 0, 8, c);
  spriteWifiStrength->drawLine(9, 0, 1, 8, c);
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t h = 3 * (i + 1);
    spriteWifiStrength->fillRect(3 * i + 4, 16 - h, 2, h, lvl > i ? EBUS_4BIT_GREEN : EBUS_4BIT_DARKGREY);
  }
  spriteWifiStrength->pushSprite(x, y);
}

void draw_sprite_ebus_queue(int32_t x, int32_t y, uint8_t queue_size) {
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
    spriteEbusQueue->drawFastVLine(i, 7, 8, color);
  }
  spriteEbusQueue->pushSprite(x, y);
}
