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

// Number of ADC samples to capture per key for noise measurement
#define MEASUREMENT_NUM_SAMPLES 1000

// Number of rapid ADC samples for settling time measurement
#define MEASUREMENT_SETTLING_SAMPLES 200

// Number of samples for average measurement
#define MEASUREMENT_AVG_SAMPLES 100

// Number of main loop iterations for looptime measurement
#define MEASUREMENT_LOOPTIME_ITERATIONS 1000

/**
 * @brief Process CDC input and run measurement tasks
 *
 * Call this from the main loop after tud_task().
 */
void measurement_task(void);
