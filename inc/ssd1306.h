#ifndef SSD1306_H
#define SSD1306_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_I2C_ADDR 0x3C

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t pages;
    uint8_t address;
    i2c_inst_t *i2c;
    uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
} ssd1306_t;

void ssd1306_init(ssd1306_t *ssd, uint8_t width, uint8_t height, uint8_t address, i2c_inst_t *i2c);
void ssd1306_send_data(ssd1306_t *ssd);
void ssd1306_fill(ssd1306_t *ssd, bool value);
void ssd1306_draw_pixel(ssd1306_t *ssd, uint8_t x, uint8_t y, bool value);
void ssd1306_draw_string(ssd1306_t *ssd, uint8_t x, uint8_t y, const char *str);

#endif
