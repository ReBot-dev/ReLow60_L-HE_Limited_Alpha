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

#include "matrix.h"

#include "distance.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "lib/bitmap.h"

// Exponential moving average (EMA) filter
#define EMA(x, y)                                                              \
  (((uint32_t)(x) +                                                            \
    ((uint32_t)(y) * ((1 << MATRIX_EMA_ALPHA_EXPONENT) - 1))) >>               \
   MATRIX_EMA_ALPHA_EXPONENT)

__attribute__((always_inline)) static inline uint16_t
matrix_analog_read(uint8_t key) {
  uint16_t raw = analog_read(key);
#if defined(MATRIX_INVERT_ADC_VALUES)
  raw = ADC_MAX_VALUE - raw;
#endif
#if defined(MATRIX_INVERT_ADC_KEYS_BITMAP)
  if ((1u << key) & MATRIX_INVERT_ADC_KEYS_BITMAP)
    raw = ADC_MAX_VALUE - raw;
#endif
  return raw;
}

__attribute__((always_inline)) static inline uint16_t
matrix_bottom_out_value(uint8_t key, uint16_t rest_value) {
  uint16_t initial = eeconfig->calibration.initial_bottom_out_threshold;
  uint8_t sw = eeconfig->switch_map[key];
  if (sw > 0 && sw < SWITCH_TYPE_COUNT)
    initial = switch_initial_bottom_out[sw];
  return M_MIN(rest_value + M_MAX(initial, eeconfig->bottom_out_threshold[key]),
               ADC_MAX_VALUE);
}

// Initial rest value used to seed the start-up calibration. The calibration
// only converges the rest value downward, so this must be at or above the
// actual rest value of the switch. A per-switch safeguard upper bound is used
// when a switch type is assigned; otherwise the global value is used.
__attribute__((always_inline)) static inline uint16_t
matrix_initial_rest_value(uint8_t key) {
  uint8_t sw = eeconfig->switch_map[key];
  if (sw > 0 && sw < SWITCH_TYPE_COUNT && switch_initial_rest_value[sw] > 0)
    return switch_initial_rest_value[sw];
  return eeconfig->calibration.initial_rest_value;
}

key_state_t key_matrix[NUM_KEYS];

// Bitmap for tracking which keys have Rapid Trigger disabled
static bitmap_t rapid_trigger_disabled[] = MAKE_BITMAP(NUM_KEYS);

void matrix_init(void) { matrix_recalibrate(false); }

void matrix_recalibrate(bool reset_bottom_out_threshold) {
  if (reset_bottom_out_threshold) {
    uint16_t bottom_out_threshold[NUM_KEYS] = {0};
    EECONFIG_WRITE(bottom_out_threshold, bottom_out_threshold);
  }

  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    const uint16_t initial_rest = matrix_initial_rest_value(i);
    key_matrix[i].adc_filtered = initial_rest;
    key_matrix[i].adc_rest_value = initial_rest;
    key_matrix[i].adc_bottom_out_value = matrix_bottom_out_value(i, initial_rest);
    key_matrix[i].distance = 0;
    key_matrix[i].extremum = 0;
    key_matrix[i].key_dir = KEY_DIR_INACTIVE;
    key_matrix[i].is_pressed = false;
    key_matrix[i].release_since = 0;
    key_matrix[i].last_rebaseline = 0;
  }

  // We only calibrate the rest value. The bottom-out value will be updated
  // during the scan process.
  const uint32_t calibration_start = timer_read();
  while (timer_elapsed(calibration_start) < MATRIX_CALIBRATION_DURATION) {
    // Run the analog task to possibly update the ADC values
    analog_task();

    for (uint32_t i = 0; i < NUM_KEYS; i++) {
      const uint16_t new_adc_filtered =
          EMA(matrix_analog_read(i), key_matrix[i].adc_filtered);

      key_matrix[i].adc_filtered = new_adc_filtered;

      if (new_adc_filtered + MATRIX_CALIBRATION_EPSILON <=
          key_matrix[i].adc_rest_value)
        // Only update the rest value if the new value is smaller and the
        // difference is at least the calibration epsilon
        key_matrix[i].adc_rest_value = new_adc_filtered;

      // Update the bottom-out value to be the minimum bottom-out value based on
      // the updated rest value
      key_matrix[i].adc_bottom_out_value =
          matrix_bottom_out_value(i, key_matrix[i].adc_rest_value);
    }
  }
}

void matrix_scan(void) {
  const uint32_t now = timer_read();
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    const uint16_t new_adc_filtered =
        EMA(matrix_analog_read(i), key_matrix[i].adc_filtered);
    const actuation_t *actuation = &CURRENT_PROFILE.actuation_map[i];

    key_matrix[i].adc_filtered = new_adc_filtered;

    if (new_adc_filtered >=
        key_matrix[i].adc_bottom_out_value + MATRIX_CALIBRATION_EPSILON)
      // Only update the bottom-out value if the new value is larger and the
      // difference is at least the calibration epsilon.
      key_matrix[i].adc_bottom_out_value = new_adc_filtered;

    key_matrix[i].distance =
        adc_to_distance(new_adc_filtered, key_matrix[i].adc_rest_value,
                        key_matrix[i].adc_bottom_out_value);

#ifdef MATRIX_FIXED_ONOFF_KEYS
    {
      // Hardware workaround: keys listed here have too little ADC travel for
      // normal analog actuation (e.g. a large magnet-sensor gap). They bypass
      // the configured actuation point / Rapid Trigger entirely and act as a
      // plain ON/OFF switch using a raw-ADC Schmitt trigger on the deviation
      // from the resting value. The configured AP/RP are still stored and shown
      // in the web configurator, but have no effect on these keys.
      static const uint8_t fixed_onoff_keys[] = MATRIX_FIXED_ONOFF_KEYS;
      bool is_fixed_onoff = false;
      for (uint32_t k = 0; k < sizeof(fixed_onoff_keys); k++)
        if (fixed_onoff_keys[k] == i) {
          is_fixed_onoff = true;
          break;
        }
      if (is_fixed_onoff) {
        const uint16_t rest = key_matrix[i].adc_rest_value;
        const uint16_t delta =
            new_adc_filtered > rest ? (uint16_t)(new_adc_filtered - rest) : 0;
        if (key_matrix[i].is_pressed) {
          if (delta <= MATRIX_FIXED_ONOFF_OFF_DELTA)
            key_matrix[i].is_pressed = false;
        } else if (delta >= MATRIX_FIXED_ONOFF_ON_DELTA) {
          key_matrix[i].is_pressed = true;
        }
        key_matrix[i].key_dir = KEY_DIR_INACTIVE;
        continue;
      }
    }
#endif

    if (bitmap_get(rapid_trigger_disabled, i) | (actuation->rt_down == 0)) {
      key_matrix[i].key_dir = KEY_DIR_INACTIVE;
      key_matrix[i].is_pressed =
          (key_matrix[i].distance >= actuation->actuation_point);
    } else {
      const uint8_t reset_point =
          actuation->continuous ? 0 : actuation->actuation_point;
      const uint8_t rt_up =
          actuation->rt_up == 0 ? actuation->rt_down : actuation->rt_up;

      switch (key_matrix[i].key_dir) {
      case KEY_DIR_INACTIVE:
        if (key_matrix[i].distance > actuation->actuation_point) {
          // Pressed down past actuation point
          key_matrix[i].extremum = key_matrix[i].distance;
          key_matrix[i].key_dir = KEY_DIR_DOWN;
          key_matrix[i].is_pressed = true;
        }
        break;

      case KEY_DIR_DOWN:
        if (key_matrix[i].distance <= reset_point) {
          // Released past reset point
          key_matrix[i].extremum = key_matrix[i].distance;
          key_matrix[i].key_dir = KEY_DIR_INACTIVE;
          key_matrix[i].is_pressed = false;
        } else if (key_matrix[i].distance + rt_up < key_matrix[i].extremum) {
          // Released by Rapid Trigger
          key_matrix[i].extremum = key_matrix[i].distance;
          key_matrix[i].key_dir = KEY_DIR_UP;
          key_matrix[i].is_pressed = false;
        } else if (key_matrix[i].distance > key_matrix[i].extremum)
          // Pressed down further
          key_matrix[i].extremum = key_matrix[i].distance;
        break;

      case KEY_DIR_UP:
        if (key_matrix[i].distance <= reset_point) {
          // Released past reset point
          key_matrix[i].extremum = key_matrix[i].distance;
          key_matrix[i].key_dir = KEY_DIR_INACTIVE;
          key_matrix[i].is_pressed = false;
        } else if (key_matrix[i].extremum + actuation->rt_down <
                   key_matrix[i].distance) {
          // Pressed by Rapid Trigger
          key_matrix[i].extremum = key_matrix[i].distance;
          key_matrix[i].key_dir = KEY_DIR_DOWN;
          key_matrix[i].is_pressed = true;
        } else if (key_matrix[i].distance < key_matrix[i].extremum)
          // Released further
          key_matrix[i].extremum = key_matrix[i].distance;
        break;

      default:
        break;
      }
    }

    // Dynamic rest re-baselining: when a key has been stably released for a
    // while, slowly track its rest value (and shift the bottom-out value by the
    // same amount to keep the range) toward the current filtered ADC value.
    // This absorbs thermal drift and aging without a dead zone or hysteresis.
    // The tracking is intentionally slow and only runs while the key is truly
    // released, so it never interferes with an actual keystroke. Tracking is
    // bidirectional, so it follows drift both upward and downward.
    if (key_matrix[i].key_dir == KEY_DIR_INACTIVE && !key_matrix[i].is_pressed) {
      if (key_matrix[i].release_since == 0)
        key_matrix[i].release_since = now;
      else if (timer_elapsed(key_matrix[i].release_since) >=
                   MATRIX_REBASELINE_DELAY_MS &&
               timer_elapsed(key_matrix[i].last_rebaseline) >=
                   MATRIX_REBASELINE_INTERVAL_MS) {
        int32_t d = (int32_t)key_matrix[i].adc_filtered -
                    (int32_t)key_matrix[i].adc_rest_value;
        int32_t step = M_MAX(-MATRIX_REBASELINE_MAX_STEP,
                             M_MIN(MATRIX_REBASELINE_MAX_STEP, d));
        if (step != 0) {
          key_matrix[i].adc_rest_value =
              (uint16_t)((int32_t)key_matrix[i].adc_rest_value + step);
          int32_t nb = (int32_t)key_matrix[i].adc_bottom_out_value + step;
          key_matrix[i].adc_bottom_out_value =
              (uint16_t)M_MAX(0, M_MIN(ADC_MAX_VALUE, nb));
          key_matrix[i].last_rebaseline = now;
        }
      }
    } else {
      // Pressed or moving: freeze the rest value.
      key_matrix[i].release_since = 0;
    }
  }
}

void matrix_disable_rapid_trigger(uint8_t key, bool disable) {
  bitmap_set(rapid_trigger_disabled, key, disable);
}
