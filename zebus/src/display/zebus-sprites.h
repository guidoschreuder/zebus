#ifndef ZEBUS_SPRITES_H
#define ZEBUS_SPRITES_H

#include "zebus-config.h"
#include <TFT_eSPI.h>

void init_sprites(TFT_eSPI *tft);

void draw_sprite_shower(int32_t x, int32_t y, float flow);
void draw_sprite_heater(int32_t x, int32_t y, bool on);
void draw_sprite_wifi_strength(int32_t x, int32_t y, int32_t rssi);
void draw_sprite_ebus_queue(int32_t x, int32_t y, uint8_t queue_size);

#endif
