/*
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "common.h"

#ifdef OLED_ENABLED

//--------------------------------------------------------------------+
// OLED Configuration
//--------------------------------------------------------------------+

#define OLED_NATIVE_WIDTH 128
#define OLED_NATIVE_HEIGHT 32
#define OLED_BUFFER_SIZE (OLED_NATIVE_WIDTH * OLED_NATIVE_HEIGHT / 8)

// Logical dimensions after 90-degree rotation
#define OLED_WIDTH 32
#define OLED_HEIGHT 128
#define OLED_I2C_ADDR 0x3C

//--------------------------------------------------------------------+
// I2C API (hardware-specific)
//--------------------------------------------------------------------+

void oled_i2c_init(void);
void oled_i2c_write(uint8_t addr, const uint8_t *data, uint16_t len);
void oled_i2c_write_dma(uint8_t addr, const uint8_t *data, uint16_t len);
bool oled_i2c_dma_busy(void);

//--------------------------------------------------------------------+
// OLED API
//--------------------------------------------------------------------+

void oled_init(void);
void oled_clear(void);
void oled_update(void);
bool oled_update_poll(void);
void oled_set_pixel(uint8_t x, uint8_t y, bool on);
void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void oled_draw_char(uint8_t x, uint8_t y, char c);
void oled_draw_char_color(uint8_t x, uint8_t y, char c, bool on);
void oled_draw_string(uint8_t x, uint8_t y, const char *str);
void oled_draw_string_color(uint8_t x, uint8_t y, const char *str, bool on);
void oled_draw_number(uint8_t x, uint8_t y, uint16_t num);
void oled_draw_number_color(uint8_t x, uint8_t y, uint16_t num, bool on);

#endif
