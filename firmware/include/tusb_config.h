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

#if !defined(CFG_TUSB_MCU)
#error "CFG_TUSB_MCU is not defined"
#endif

// Default TinyUSB configuration
#define CFG_TUSB_OS OPT_OS_NONE
#define CFG_TUSB_DEBUG 0
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#define CFG_TUD_ENABLED 1
#define CFG_TUD_ENDPOINT0_SIZE 64

// Driver configuration
// Keyboard, generic, and raw HID interfaces
#define CFG_TUD_HID 3
// CDC (virtual serial port) for debug/measurement output
#define CFG_TUD_CDC 1

// HID buffer size. Must be at least the size of the largest reports (+1 for
// interface with multiple reports)
#define CFG_TUD_HID_EP_BUFSIZE 64

#if defined(BOARD_USB_FS)
#define BOARD_TUD_RHPORT 0
#define CDC_DATA_EP_SIZE 64
#elif defined(BOARD_USB_HS)
#define BOARD_TUD_RHPORT 1
// USB HS bulk endpoints require maxpacket 512
#define CDC_DATA_EP_SIZE 512
#else
#error "USB peripheral not defined"
#endif

// CDC FIFO buffers must be >= endpoint size for HS
#define CFG_TUD_CDC_RX_BUFSIZE CDC_DATA_EP_SIZE
#define CFG_TUD_CDC_TX_BUFSIZE (CDC_DATA_EP_SIZE * 4)
