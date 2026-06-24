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

//--------------------------------------------------------------------+
// Macro Playback
//--------------------------------------------------------------------+

// Macro event opcodes. A macro is a sequence of 2-byte events
// (`[opcode, arg]`) stored in `eeconfig->macros`, terminated by a single
// `MACRO_OP_END` byte. Macros are stored back-to-back, so macro N is the
// (N+1)-th terminated sequence in the buffer. An empty macro is just a lone
// `MACRO_OP_END`. The configurator is responsible for laying out the buffer
// with at least `MACRO_COUNT` terminators.
typedef enum {
  // End of the macro (1 byte, no argument)
  MACRO_OP_END = 0x00,
  // Tap (press then release) the keycode in `arg`
  MACRO_OP_TAP = 0x01,
  // Press (and hold) the keycode in `arg`
  MACRO_OP_DOWN = 0x02,
  // Release the keycode in `arg`
  MACRO_OP_UP = 0x03,
  // Wait `arg` * `MACRO_DELAY_UNIT_MS` milliseconds
  MACRO_OP_DELAY = 0x04,
} macro_op_t;

// Milliseconds represented by one unit of a `MACRO_OP_DELAY` argument. With a
// single byte argument this gives a 0..2550 ms range per delay event; longer
// waits are expressed with consecutive delay events.
#define MACRO_DELAY_UNIT_MS 10

/**
 * @brief Initialize the macro playback module
 *
 * @return None
 */
void macro_init(void);

/**
 * @brief Start playing the macro at `index`
 *
 * Has no effect if a macro is already playing, the index is out of range, or
 * the macro is empty.
 *
 * @param index Macro index (0 .. MACRO_COUNT - 1)
 *
 * @return None
 */
void macro_play(uint8_t index);

/**
 * @brief Advance macro playback
 *
 * Should be called once per matrix scan. Emits at most one HID-changing event
 * per `MACRO_STEP_MS` so the host registers each press/release.
 *
 * @return None
 */
void macro_task(void);
