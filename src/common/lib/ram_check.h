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

#endif // RAM_CHECK_H
