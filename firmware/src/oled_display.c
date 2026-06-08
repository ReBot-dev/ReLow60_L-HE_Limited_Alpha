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

#ifdef OLED_ENABLED

#include "oled_display.h"

#include "hardware/oled_api.h"
#include "hardware/timer_api.h"
#include "matrix.h"

// Display layout for 32x128 OLED (128x32 panel rotated 90 degrees)
//
// Top: 3x3 grid of key shapes with fill animation
// Bottom: ADC values in a single column (inverted when pressed)
//
// Key rectangle size: 8x8 pixels
// Grid spacing: 10px horizontal, 10px vertical

#define KEY_W 8
#define KEY_H 8
#define GRID_X 1
#define GRID_Y 2
#define GRID_STEP_X 10
#define GRID_STEP_Y 10

// ADC number display area (single column below key grid)
#define NUM_Y 38
#define NUM_STEP_Y 10
#define NUM_LINE_H 8

// Update interval in milliseconds (~30 FPS)
#define OLED_UPDATE_INTERVAL_MS 33

// State machine for non-blocking OLED update
enum { OLED_STATE_IDLE, OLED_STATE_SENDING };

static uint32_t last_update;
static uint8_t oled_state = OLED_STATE_IDLE;

void oled_display_init(void) {
  oled_init();
  last_update = timer_read();
}

void oled_display_task(void) {
  switch (oled_state) {
  case OLED_STATE_IDLE:
    if (timer_elapsed(last_update) < OLED_UPDATE_INTERVAL_MS)
      return;
    last_update = timer_read();

    // Render framebuffer
    oled_clear();

    uint8_t num_keys = NUM_KEYS;
    if (num_keys > 9)
      num_keys = 9;

    for (uint8_t i = 0; i < num_keys; i++) {
      uint8_t col = i / 3;
      uint8_t row = i % 3;

      uint8_t kx = GRID_X + col * GRID_STEP_X;
      uint8_t ky = GRID_Y + row * GRID_STEP_Y;

      // Key fill animation: fill from top proportional to distance
      oled_draw_rect(kx, ky, KEY_W, KEY_H);
      uint8_t distance = key_matrix[i].distance;
      if (distance > 0) {
        uint8_t fill_h =
            (uint8_t)(1 + (uint16_t)distance * (KEY_H - 1) / 255);
        oled_fill_rect(kx, ky, KEY_W, fill_h, true);
      }

      // ADC value display with key index label
      uint8_t ny = NUM_Y + i * NUM_STEP_Y;
      if (key_matrix[i].is_pressed) {
        // Inverted display when pressed
        oled_fill_rect(0, ny, OLED_WIDTH, NUM_LINE_H, true);
        oled_draw_char_color(0, ny, '1' + i, false);
        oled_draw_number_color(8, ny, key_matrix[i].adc_filtered, false);
      } else {
        oled_draw_char(0, ny, '1' + i);
        oled_draw_number(8, ny, key_matrix[i].adc_filtered);
      }
    }

    // Start DMA page transfers (sends commands, then kicks off page 0)
    oled_update();
    oled_state = OLED_STATE_SENDING;
    // Fall through to immediately try sending first page
    __attribute__((fallthrough));

  case OLED_STATE_SENDING:
    if (oled_update_poll())
      oled_state = OLED_STATE_IDLE;
    return;

  default:
    break;
  }
}

#endif
