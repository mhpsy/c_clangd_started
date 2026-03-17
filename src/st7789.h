#pragma once
#include <stdint.h>
#include "ch347.h"

/* Adjust for your display */
#define ST7789_WIDTH  240
#define ST7789_HEIGHT 240

/* RGB565 colors */
#define ST7789_BLACK  0x0000
#define ST7789_WHITE  0xFFFF
#define ST7789_RED    0xF800
#define ST7789_GREEN  0x07E0
#define ST7789_BLUE   0x001F

void st7789_init(ch347_dev_t *dev);
void st7789_fill(ch347_dev_t *dev, uint16_t color);
