
#include "ebus-display.h"

TFT_eSPI tft = TFT_eSPI(); // Invoke library
TFT_eSprite spriteShower = TFT_eSprite(&tft);

TFT_eSprite spriteHeater = TFT_eSprite(&tft);
uint16_t heaterWaveOffset = 0;

TFT_eSprite spriteFlame = TFT_eSprite(&tft);

// TODO: remove the test variables
uint8_t flow = 0;
uint8_t flow_cntr = 0;

// 4-bit Palette colour table
uint16_t palette[16];

void init4BitPallet() {
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
#define EBUS_4BIT_ORANGE 8
#define EBUS_4BIT_CYAN 11
#define EBUS_4BIT_RED 12
#define EBUS_4BIT_NAvY 13
#define EBUS_4BIT_WHITE 15

void initSpriteShower() {
    spriteShower.setColorDepth(4);
    spriteShower.createSprite(30,30);
    spriteShower.createPalette(palette);
    spriteShower.fillSprite(EBUS_4BIT_BLACK);
}

void drawSpriteShower(int32_t x, int32_t y, uint8_t flow) {
  // remove previous water
  spriteShower.fillRect(10, 12, 20, 18, EBUS_4BIT_BLACK);

  spriteShower.fillRoundRect(16, 0, 8, 12, 1, flow ? EBUS_4BIT_RED : EBUS_4BIT_BLACK);
  spriteShower.fillRoundRect(0, 2, 14, 4, 1, flow ? EBUS_4BIT_RED : EBUS_4BIT_BLACK);
  if (!flow) {
    spriteShower.drawRoundRect(16, 0, 8, 12, 1, EBUS_4BIT_WHITE);
    spriteShower.drawRoundRect(0, 2, 14, 4, 1, EBUS_4BIT_WHITE);
  } else {
    for (uint8_t i = 0; i < 2 + (flow / 2); i++) {
      uint16_t w_y = random(16);
      uint16_t w_x = random((flow * w_y  / 512) + 3);
      w_x = 20 + (random(2) ? w_x : -w_x);
      uint8_t c = EBUS_4BIT_CYAN;
      uint8_t rnd = random(256);
      if (rnd < 32) {
        c = EBUS_4BIT_DARKCYAN;
      } else if (rnd < 64) {
        c = EBUS_4BIT_NAvY;
      }
      spriteShower.drawLine(w_x, w_y + 12, w_x, w_y + 14, c);
    }
  }
  spriteShower.pushSprite(x, y);
}

void initSpriteHeater() {
    spriteHeater.setColorDepth(4);
    spriteHeater.createSprite(30,30);
    spriteHeater.createPalette(palette);
    spriteHeater.fillSprite(EBUS_4BIT_BLACK);
}

void drawSpriteHeater(int32_t x, int32_t y, bool on) {
  const uint16_t WAVE_Y_OFFSET = 5;
  spriteHeater.fillRect(0, 0, 30, WAVE_Y_OFFSET + 1, EBUS_4BIT_BLACK);
  for (int i = 0; i < 4; i++) {
    spriteHeater.drawRoundRect(8 * i, WAVE_Y_OFFSET, 5, 25, 1, on ? EBUS_4BIT_ORANGE : EBUS_4BIT_WHITE);
    if (i < 3) {
      spriteHeater.drawFastHLine(8 * i + 5, WAVE_Y_OFFSET + 4, 3, on ? EBUS_4BIT_ORANGE : EBUS_4BIT_WHITE);
      spriteHeater.drawFastHLine(8 * i + 5, WAVE_Y_OFFSET + 20, 3, on ? EBUS_4BIT_ORANGE : EBUS_4BIT_WHITE);
    }
  }
  if (on) {
    uint8_t wave_y_prev = WAVE_Y_OFFSET - 3, wave_width = 29;
    for (uint8_t wave_x = 0; wave_x < wave_width; wave_x++) {
      uint8_t wave_y = WAVE_Y_OFFSET - 3 + sinf((wave_x + heaterWaveOffset)/ 2.0) * 2;
      spriteHeater.drawLine(wave_x - 1, wave_y_prev, wave_x, wave_y, EBUS_4BIT_RED);
      wave_y_prev = wave_y;
    }
    heaterWaveOffset++;
  }
  spriteHeater.pushSprite(x, y);
}

void initSpriteFlame() {
  spriteFlame.createSprite(TFT_HEIGHT, 30);
  initFlame();
}

void initSprites() {
  init4BitPallet();
  initSpriteShower();
  initSpriteHeater();
  initSpriteFlame();
}

void setupDisplay() {
  printf("Setup TFT\n");
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Set "cursor" at top left corner of display (0,0) and select font 1
  tft.setCursor(0, 100, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  for(char c = 0; c < 127; c++) {
    tft.print(c);
  }

  initSprites();
}

void updateDisplay(void *pvParameter) {
  while(1) {

    if (++flow_cntr % 4 == 0) {
      flow += 4;
    }

    drawSpriteHeater(280, 10, true);
    drawSpriteHeater(240, 10, false);
    drawSpriteShower(280, 50, flow);
    drawSpriteShower(240, 50, 0);

    make_fire(&spriteFlame);
    spriteFlame.pushSprite(0, 210);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}