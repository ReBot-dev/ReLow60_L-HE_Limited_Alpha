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

#include "macro.h"

#include "eeconfig.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "keycodes.h"

_Static_assert(MACRO_COUNT <= (SP_MACRO_MAX - SP_MACRO_MIN + 1),
               "MACRO_COUNT exceeds the macro keycode range");

// Minimum time in milliseconds between two HID-changing macro steps. This keeps
// each press/release on a distinct USB frame so the host registers taps
// reliably, and also serves as the implicit per-event timing of the macro.
#define MACRO_STEP_MS 4

static bool macro_active;
// Read position of the next event within `eeconfig->macros`.
static uint16_t macro_pos;
// Keycode to release on the next step to complete a tap, or `KC_NO`.
static uint8_t macro_pending_release;
// Whether playback is currently honoring a `MACRO_OP_DELAY`.
static bool macro_waiting;
// Timestamp of the last step (or the start of the current wait).
static uint32_t macro_timer;
// Remaining wait duration in milliseconds for the current delay.
static uint16_t macro_wait_ms;

void macro_init(void) {
  macro_active = false;
  macro_pending_release = KC_NO;
  macro_waiting = false;
}

void macro_play(uint8_t index) {
  if (macro_active || index >= MACRO_COUNT)
    // Only one macro plays at a time.
    return;

  // Locate the start of the `index`-th macro by skipping that many terminators.
  uint16_t pos = 0;
  for (uint8_t n = 0; n < index; n++) {
    while (pos < MACRO_BUFFER_SIZE && eeconfig->macros[pos] != MACRO_OP_END)
      pos += 2;
    if (pos >= MACRO_BUFFER_SIZE)
      // Fewer macros stored than requested.
      return;
    // Skip the terminator.
    pos += 1;
  }
  if (pos >= MACRO_BUFFER_SIZE || eeconfig->macros[pos] == MACRO_OP_END)
    // Empty macro.
    return;

  macro_active = true;
  macro_pos = pos;
  macro_pending_release = KC_NO;
  macro_waiting = false;
  macro_timer = timer_read();
}

void macro_task(void) {
  if (!macro_active)
    return;

  // Honor an explicit delay.
  if (macro_waiting) {
    if (timer_elapsed(macro_timer) < macro_wait_ms)
      return;
    macro_waiting = false;
    macro_timer = timer_read();
  }

  // Rate-limit HID-changing steps so each event lands on its own USB frame.
  if (timer_elapsed(macro_timer) < MACRO_STEP_MS)
    return;

  // Complete a pending tap by releasing the key.
  if (macro_pending_release != KC_NO) {
    hid_keycode_remove(macro_pending_release);
    macro_pending_release = KC_NO;
    hid_send_reports();
    macro_timer = timer_read();
    return;
  }

  if (macro_pos + 1 >= MACRO_BUFFER_SIZE) {
    // Ran off the end of the buffer without a terminator.
    macro_active = false;
    return;
  }

  const uint8_t op = eeconfig->macros[macro_pos];
  if (op == MACRO_OP_END) {
    macro_active = false;
    return;
  }

  const uint8_t arg = eeconfig->macros[macro_pos + 1];
  macro_pos += 2;

  switch (op) {
  case MACRO_OP_TAP:
    hid_keycode_add(arg);
    // Release on the next step so the press and release are separate reports.
    macro_pending_release = arg;
    hid_send_reports();
    macro_timer = timer_read();
    break;

  case MACRO_OP_DOWN:
    hid_keycode_add(arg);
    hid_send_reports();
    macro_timer = timer_read();
    break;

  case MACRO_OP_UP:
    hid_keycode_remove(arg);
    hid_send_reports();
    macro_timer = timer_read();
    break;

  case MACRO_OP_DELAY:
    macro_waiting = true;
    macro_wait_ms = (uint16_t)arg * MACRO_DELAY_UNIT_MS;
    macro_timer = timer_read();
    break;

  default:
    // Unknown opcode: abort to avoid runaway behavior.
    macro_active = false;
    break;
  }
}
