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

/**
 * @file ram_check.h
 * @brief Runtime verification that code is executing from SRAM
 *
 * Provides compile-time optional runtime assertions to verify firmware
 * is executing from SRAM rather than Flash memory.
 *
 * @see ram_check.c for implementation details
 */

#ifndef RAM_CHECK_H
#define RAM_CHECK_H

/**
 * @brief Verify that code is executing from SRAM
 *
 * This function checks that the calling code is executing from SRAM
 * (0x20000000-0x20042000) rather than Flash (0x10000000-0x15FFFFFF).
 *
 * Behavior:
 * - If RUN_FROM_RAM_CHECK is defined: Panics if executing from Flash
 * - If RUN_FROM_RAM_CHECK is not defined: No-op (zero overhead)
 *
 * Usage:
 * ```c
 * int main(void) {
 *     ram_check_verify();  // Call early in main()
 *     // ... rest of initialization
 * }
 * ```
 *
 * @note Only active in debug builds when RUN_FROM_RAM_CHECK is defined
 * @note Zero overhead in release builds (compiled out)
 * @note Will panic() with descriptive message if check fails
 */
void ram_check_verify(void);

#endif  // RAM_CHECK_H
