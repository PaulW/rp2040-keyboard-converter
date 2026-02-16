/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023-2026 Paul Bramhall (paulwamp@gmail.com)
 *
 * RP2040 Keyboard Converter is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * RP2040 Keyboard Converter is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RP2040 Keyboard Converter.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "ws2812.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ws2812.pio.h"  // Generated from ws2812.pio at build time

#include "bsp/board.h"

#include "config.h"
#include "log.h"
#include "pio_helper.h"

uint ws2812_sm = 0;
uint ws2812_offset = 0;
PIO ws2812_pio = NULL;

/*
 * Private Functions
 */

/**
 * @brief Gamma-Corrected Brightness Lookup Table
 * 
 * This lookup table provides perceptually linear brightness control for WS2812 LEDs
 * by applying gamma correction. Human perception of brightness is non-linear - doubling
 * the electrical brightness does not result in perceived doubling of light intensity.
 * 
 * Gamma Correction Theory:
 * - Human eye response follows a power curve (approximately γ = 2.2-2.8)
 * - Linear brightness steps (e.g., 25, 50, 75, 100) appear uneven to the eye
 * - Gamma correction compensates by using a power function: output = input^γ
 * - Result: Each brightness step appears perceptually uniform
 * 
 * Table Design:
 * - Input range: 1-10 (user-facing brightness levels)
 * - Output range: 0-255 (8-bit PWM duty cycle multiplier)
 * - Index 0 is unused (brightness levels start at 1, not 0)
 * - Calibrated for approximately γ = 2.5 power curve
 * - Values hand-tuned for smooth visual progression
 * 
 * Usage Pattern:
 * ```c
 * uint8_t user_brightness = 3;  // Range 1-10
 * uint8_t multiplier = BRIGHTNESS_LUT[user_brightness];  // → 10
 * uint8_t adjusted_red = (red_value * multiplier) / 255;
 * ```
 * 
 * Performance Characteristics:
 * - Lookup time: O(1) - single array access
 * - Memory: 11 bytes (const, stored in flash not RAM)
 * - Static linkage: Internal to this compilation unit only
 * - Zero computational overhead vs real-time gamma calculation
 * 
 * Brightness Curve Analysis:
 * | Level | Multiplier | % of Max | Perceived | Step Size |
 * |-------|------------|----------|-----------|-----------|
 * |   1   |     2      |   0.8%   |   ~10%    |    -      |
 * |   2   |     5      |   2.0%   |   ~20%    |  ~10%     |
 * |   3   |    10      |   3.9%   |   ~30%    |  ~10%     |
 * |   4   |    20      |   7.8%   |   ~40%    |  ~10%     |
 * |   5   |    35      |  13.7%   |   ~50%    |  ~10%     |
 * |   6   |    60      |  23.5%   |   ~60%    |  ~10%     |
 * |   7   |    90      |  35.3%   |   ~70%    |  ~10%     |
 * |   8   |   135      |  52.9%   |   ~80%    |  ~10%     |
 * |   9   |   190      |  74.5%   |   ~90%    |  ~10%     |
 * |  10   |   255      | 100.0%   |  ~100%    |  ~10%     |
 * 
 * Alternative Approaches (Not Used):
 * - Real-time calculation: `pow(input/10.0, 2.5) * 255` - Too slow (uses float/libm)
 * - Linear scaling: `input * 25.5` - Perceptually uneven, fails at low brightness
 * - Larger LUT: More granularity - Unnecessary for 10-level user control
 * 
 * Calibration Notes:
 * - Low end (1-3): Critical for dim ambient lighting, careful logarithmic spacing
 * - Mid range (4-7): Balanced progression for typical indoor use
 * - High end (8-10): Aggressive scaling to reach full brightness smoothly
 * - Level 1 intentionally dim (0.8%) for night use / dark environments
 * - Level 10 = 255 ensures full LED capability is accessible
 * 
 * @note Array is 0-indexed but brightness levels are 1-indexed (1-10)
 * @note Index 0 contains 0 (unused) - attempting brightness=0 yields black
 * @note Static storage in flash (const) - no RAM consumption
 * @note Compiler may optimize away unused table entries if brightness is compile-time constant
 */
#ifdef CONVERTER_LEDS
static const uint8_t BRIGHTNESS_LUT[11] = {
    0,   // Index 0: Unused (brightness levels start at 1)
    2,   // Level 1: Very dim    (  0.8% electrical = ~10% perceived)
    5,   // Level 2: Quite dim   (  2.0% electrical = ~20% perceived)
    10,  // Level 3: Dim         (  3.9% electrical = ~30% perceived)
    20,  // Level 4: Low         (  7.8% electrical = ~40% perceived)
    35,  // Level 5: Medium-low  ( 13.7% electrical = ~50% perceived)
    60,  // Level 6: Medium      ( 23.5% electrical = ~60% perceived)
    90,  // Level 7: Medium-high ( 35.3% electrical = ~70% perceived)
    135, // Level 8: Bright      ( 52.9% electrical = ~80% perceived)
    190, // Level 9: Very bright ( 74.5% electrical = ~90% perceived)
    255  // Level 10: Maximum    (100.0% electrical = 100% perceived)
};
#endif

/**
 * @brief Runtime LED brightness level (0-10)
 * 
 * Global variable to track current LED brightness setting. This allows runtime
 * adjustment of brightness without recompiling firmware.
 * 
 * - Range: 0-10 (0=off, 10=max)
 * - Default: From CONVERTER_LEDS_BRIGHTNESS in config.h (compile-time)
 * - Thread-safety: All access from main loop context only
 * - Used by ws2812_set_color() to look up multiplier from BRIGHTNESS_LUT
 * 
 * Initialization:
 * - Starts with compile-time default from config.h
 * - Can be overridden at runtime from persistent config (see main.c)
 * - Similar pattern to log level: compile-time default, runtime override
 * 
 * @note Initialized here with compile-time default
 * @note May be overridden in main.c from config_get_led_brightness()
 * @note Modified by ws2812_set_brightness() during user adjustment
 */
#ifdef CONVERTER_LEDS
#ifndef CONVERTER_LEDS_BRIGHTNESS
#error "CONVERTER_LEDS_BRIGHTNESS must be defined in config.h"
#endif
static uint8_t g_led_brightness = CONVERTER_LEDS_BRIGHTNESS;
#endif

/**
 * @brief Applies gamma-corrected brightness and color order to LED color value
 * 
 * This function performs two critical transformations on the input RGB color:
 * 
 * 1. **Gamma-Corrected Brightness Scaling**:
 *    - Looks up perceptually-linear multiplier from BRIGHTNESS_LUT
 *    - Scales each RGB component: `output = (input * multiplier) / 255`
 *    - Division by 255 normalizes the result back to 8-bit range (0-255)
 *    - Uses integer arithmetic for performance (no floating point)
 * 
 * 2. **Color Order Conversion**:
 *    - Different WS2812-compatible LEDs use different color orders
 *    - Common variants: RGB, GRB (most common), BGR, RBG, GBR, BRG
 *    - Function reorders color bytes based on CONVERTER_LEDS_TYPE configuration
 *    - Ensures correct color output regardless of LED chip variant
 * 
 * Performance Optimizations:
 * - Function is `static inline` for potential inlining at call site
 * - Brightness multiplier lookup is O(1) array access (not calculated)
 * - Color extraction uses bit shifts and masks (fast bitwise ops)
 * - Integer-only arithmetic (no floating point or division hardware needed)
 * - Switch statement may be optimized to jump table by compiler
 * - Conditional compilation (#ifdef) eliminates unused code paths
 * 
 * Mathematical Details:
 * ```
 * Brightness scaling formula:
 *   output_component = (input_component × multiplier) ÷ 255
 * 
 * Example (brightness level 3, multiplier = 10):
 *   input_red = 255 (full intensity)
 *   output_red = (255 × 10) ÷ 255 = 2550 ÷ 255 = 10
 *   
 * Example (brightness level 10, multiplier = 255):
 *   input_red = 128 (half intensity)
 *   output_red = (128 × 255) ÷ 255 = 128 (unchanged)
 * ```
 * 
 * Why Division by 255 (Not 256):
 * - Multiplier is in range [0, 255] representing fraction of brightness
 * - Division by 255 gives exact scaling: multiplier 255 = 100%, multiplier 128 = ~50.2%
 * - Division by 256 would give: multiplier 255 = 99.6%, multiplier 128 = 50.0%
 * - Using 255 ensures multiplier=255 doesn't dim the LED by 0.4%
 * - Compiler may optimize `/255` to `(x * 0x8080 + 0x8000) >> 23` or similar
 * 
 * Color Order Examples:
 * ```
 * Input:  0x00FF8040 (R=255, G=128, B=64)
 * 
 * LED_RGB: 0x00FF8040 → [R][G][B] → Red=255, Green=128, Blue=64
 * LED_GRB: 0x0080FF40 → [G][R][B] → Green=255, Red=128, Blue=64 (most common)
 * LED_BGR: 0x004080FF → [B][G][R] → Blue=255, Green=128, Red=64
 * ```
 * 
 * Byte Order in Memory (Little-Endian RP2040):
 * ```
 * 32-bit value 0x00RRGGBB in memory:
 *   [BB] [GG] [RR] [00]  ← Lowest address
 *   byte0 byte1 byte2 byte3
 * 
 * PIO shifts out MSB first, so byte order sent to LED:
 *   [00] [RR] [GG] [BB]  ← Bit 31 first, bit 0 last
 * ```
 * 
 * Typical Call Flow:
 * ```
 * User calls: ws2812_show(0x00FF0000)  // Red in RGB format
 *             ↓
 * ws2812_set_color(0x00FF0000)
 *   → Extracts: R=255, G=0, B=0
 *   → Applies brightness (e.g., level 3): R=10, G=0, B=0
 *   → Reorders for GRB: returns 0x000A0000 (G=0, R=10, B=0)
 *             ↓
 * Value shifted left 8 bits: 0x00A00000
 *   → PIO autopull converts to GRB bit stream
 *   → LED displays dim red (gamma-corrected)
 * ```
 * 
 * @param led_color Input color in RGB format (0x00RRGGBB)
 * @return Color value converted to LED's color order with brightness applied
 * 
 * @note Input is always RGB format (user-facing)
 * @note Output format depends on CONVERTER_LEDS_TYPE configuration
 * @note Brightness is applied before color reordering
 * @note Function is static inline for performance
 * @note All calculations use integer arithmetic (fast on Cortex-M0+)
 */
static inline uint32_t ws2812_set_color(uint32_t led_color) {
  // Extract RGB components from 24-bit color value (bit shifts + masks)
  // Input format: 0x00RRGGBB
  //   Red:   bits 23-16  (0xFF0000)
  //   Green: bits 15-8   (0x00FF00)
  //   Blue:  bits 7-0    (0x0000FF)
  uint8_t r = (led_color >> 16) & 0xFF;  // Extract red:   (value >> 16) & 0xFF
  uint8_t g = (led_color >> 8) & 0xFF;   // Extract green: (value >> 8) & 0xFF
  uint8_t b = led_color & 0xFF;          // Extract blue:   value & 0xFF

  // Apply gamma-corrected brightness scaling using runtime brightness level
  // Uses g_led_brightness (0-10) to look up multiplier from BRIGHTNESS_LUT
  // This allows runtime brightness adjustment without recompiling firmware
  // 
  // Brightness level 0 special case:
  // - When g_led_brightness = 0, BRIGHTNESS_LUT[0] = 0
  // - All color components are multiplied by 0 → LED is off
  // - This is intentional: level 0 = LEDs off, level 1 = dimmest visible setting
#ifdef CONVERTER_LEDS
  if (g_led_brightness <= 10) {
    const uint8_t multiplier = BRIGHTNESS_LUT[g_led_brightness];
    
    // Scale each color component by multiplier, normalize back to 8-bit range
    // Formula: output = (input × multiplier) ÷ 255
    // 
    // Performance notes:
    // - Multiplication is fast on Cortex-M0+ (1-2 cycles for 8-bit×8-bit)
    // - Division by 255 may be optimized by compiler to multiply + shift
    // - Alternative: (x * multiplier * 257) >> 16 (faster but less accurate)
    // - Current approach prioritizes accuracy over marginal speed gain
    r = (uint8_t)((r * multiplier) / 255);
    g = (uint8_t)((g * multiplier) / 255);
    b = (uint8_t)((b * multiplier) / 255);
  }
#endif
  // If g_led_brightness > 10, use original colors unchanged (shouldn't happen, but safe fallback)

#ifdef CONVERTER_LEDS_TYPE
  // Apply color order based on LED chip variant
  // Different WS2812-compatible chips expect different byte orders:
  //   - WS2812B (most common): GRB
  //   - SK6812: GRB or RGB depending on variant
  //   - APA102: Uses SPI, not applicable here
  //   - Cheaper clones: Often BGR or other orders
  // 
  // Switch statement allows compiler to optimize to jump table
  // Only one case is included in final binary (others optimized away)
  switch (CONVERTER_LEDS_TYPE) {
    case LED_RBG:
      return (r << 16) | (b << 8) | g;  // Red-Blue-Green
    case LED_GRB:
      return (g << 16) | (r << 8) | b;  // Green-Red-Blue (most common)
    case LED_GBR:
      return (g << 16) | (b << 8) | r;  // Green-Blue-Red
    case LED_BRG:
      return (b << 16) | (r << 8) | g;  // Blue-Red-Green
    case LED_BGR:
      return (b << 16) | (g << 8) | r;  // Blue-Green-Red
    default:  // LED_RGB
      return (r << 16) | (g << 8) | b;  // Red-Green-Blue (natural order)
  }
#else
  // No color type specified - default to RGB (natural order)
  // Most readable format, but less common in actual WS2812 hardware
  return (r << 16) | (g << 8) | b;
#endif
}

/*
 * Public Functions
 */

/**
 * @brief Illuminates the WS2812 LED strip with the specified color
 * 
 * This function sets the color of a single LED in the WS2812 LED strip. It is
 * called sequentially to update multiple LEDs in a chain, with data cascading
 * through each LED to the next.
 * 
 * Non-Blocking Implementation:
 * - Checks if PIO TX FIFO has space before writing
 * - Returns false immediately if FIFO is full (no blocking)
 * - Allows caller to defer update and retry later
 * - Critical for preventing interference with time-sensitive protocol PIOs
 * 
 * WS2812 Timing:
 * - Each LED requires 24 bits (GRB format) transmitted at 800kHz
 * - Transmission takes ~30µs per LED
 * - PIO autopull (shift=24) moves data from FIFO to OSR automatically
 * - FIFO has 4-entry depth, but we check before each write for safety
 * 
 * @param led_color The color value to set for the LED (RGB format, 0x00RRGGBB)
 * @return true if color was queued to PIO, false if FIFO was full or not initialized
 * 
 * @note Caller should check return value and defer/retry if false
 * @note Color is automatically converted to GRB and adjusted for brightness
 */
bool ws2812_show(uint32_t led_color) {
  // Guard against uninitialized PIO/SM
  if (ws2812_pio == NULL) {
    return false;
  }
  
  // Check if TX FIFO has space (non-blocking approach)
  if (pio_sm_is_tx_fifo_full(ws2812_pio, ws2812_sm)) {
    // FIFO full - return false so caller can defer update
    return false;
  }
  
  // FIFO has space - queue the LED data (non-blocking)
  pio_sm_put(ws2812_pio, ws2812_sm, ws2812_set_color(led_color) << 8u);
  return true;
}

/**
 * @brief Sets up the WS2812 interface.
 * This function sets up the WS2812 interface by finding an available PIO, claiming an unused
 * state machine (SM), adding the WS2812 program to the PIO, initializing the program with the
 * specified parameters, and printing information about the setup.
 *
 * @param led_pin The GPIO pin connected to the WS2812 LED strip.
 */
void ws2812_setup(uint led_pin) {
  ws2812_pio = find_available_pio(&ws2812_program);
  if (ws2812_pio == NULL) {
    LOG_ERROR("No PIO available for WS2812 Program\n");
    return;
  }

  ws2812_sm = (uint)pio_claim_unused_sm(ws2812_pio, true);
  ws2812_offset = pio_add_program(ws2812_pio, &ws2812_program);

  float rp_clock_khz = 0.001 * clock_get_hz(clk_sys);
  float clock_div = roundf(rp_clock_khz / (800 * 10));

  LOG_INFO("Effective SM Clock Speed: %.2fkHz\n", (float)(rp_clock_khz / clock_div));

  ws2812_program_init(ws2812_pio, ws2812_sm, ws2812_offset, led_pin, clock_div);

  LOG_INFO(
      "PIO%d SM%d WS2812 Interface program loaded at offset %d with clock divider of %.2f\n",
      ws2812_pio == pio0 ? 0 : 1, ws2812_sm, ws2812_offset, clock_div);
}

/**
 * @brief Set LED brightness level
 * 
 * Updates the runtime LED brightness level. The new brightness takes effect
 * immediately on the next LED update (ws2812_show() call).
 * 
 * **Usage:**
 * - Range: 0-10 (0=off, 10=maximum brightness)
 * - Values outside range are automatically clamped
 * - Gamma correction applied via BRIGHTNESS_LUT
 * - Non-blocking operation (just updates variable)
 * 
 * **Thread Safety:**
 * - Call from main loop context only
 * - Not thread-safe (but not needed - single-threaded design)
 * 
 * @param level Brightness level (0-10, will be clamped to valid range)
 * 
 * @note Takes effect on next ws2812_show() call
 * @note Does not trigger LED update automatically
 * 
 * Example:
 * ```c
 * ws2812_set_brightness(7);  // Set to bright
 * ws2812_show(0xFF0000);     // Red at 70% perceived brightness
 * ```
 */
#ifdef CONVERTER_LEDS
void ws2812_set_brightness(uint8_t level) {
    // Clamp to valid range [0-10]
    if (level > 10) {
        level = 10;
    }
    
    g_led_brightness = level;
    LOG_DEBUG("LED brightness set to %d\n", level);
}

/**
 * @brief Get current LED brightness level
 * 
 * Returns the current runtime LED brightness setting.
 * 
 * @return Current brightness level (0-10)
 * 
 * @note Returns value from RAM (fast, no hardware access)
 * @note Thread-safe for reads
 * 
 * Example:
 * ```c
 * uint8_t current = ws2812_get_brightness();
 * if (current < 10) {
 *     ws2812_set_brightness(current + 1);  // Increment
 * }
 * ```
 */
uint8_t ws2812_get_brightness(void) {
    return g_led_brightness;
}
#endif
