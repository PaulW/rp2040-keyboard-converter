/**
 * @file ram_check.c
 * @brief Runtime verification that code is executing from SRAM
 * 
 * This module provides compile-time optional runtime assertions to verify
 * that the firmware is executing from SRAM (RAM) rather than Flash memory.
 * 
 * RP2040 Memory Map:
 * - Flash (XIP): 0x10000000 - 0x15FFFFFF
 * - SRAM:        0x20000000 - 0x20042000 (264KB)
 * 
 * The RP2040 Keyboard Converter requires all code to run from SRAM for
 * timing-critical operations (PIO protocol handling at microsecond precision).
 * This is configured in CMakeLists.txt via:
 *   pico_set_binary_type(rp2040-converter copy_to_ram)
 * 
 * This runtime check provides defense-in-depth verification that the binary
 * is actually executing from SRAM, catching build configuration errors that
 * could cause subtle timing issues.
 * 
 * Usage:
 * - Enable in debug builds: cmake -DCMAKE_BUILD_TYPE=Debug
 * - Call ram_check_verify() early in main()
 * - Will panic() if executing from Flash
 * 
 * @note Only active when RUN_FROM_RAM_CHECK is defined
 * @note Zero overhead in release builds (entirely compiled out)
 */

#include <stdint.h>
#include "pico/stdlib.h"

#ifdef RUN_FROM_RAM_CHECK

// RP2040 Memory Map Constants
#define SRAM_BASE      0x20000000u
#define SRAM_END       0x20042000u  // 264KB SRAM
#define FLASH_XIP_BASE 0x10000000u
#define FLASH_XIP_END  0x16000000u

/**
 * @brief Verify that code is executing from SRAM
 * 
 * This function uses the return address to determine where code is executing from.
 * If the return address is in Flash range (0x10000000-0x15FFFFFF), the check fails.
 * If it's in SRAM range (0x20000000-0x20042000), the check passes.
 * 
 * The function is marked __attribute__((noinline)) to ensure the return address
 * check is meaningful (inlining would make it check the caller's location instead).
 * 
 * @note Panics with descriptive message if executing from Flash
 * @note Only compiled when RUN_FROM_RAM_CHECK is defined
 */
__attribute__((noinline))
void ram_check_verify(void) {
    // Get the return address - this tells us where the calling code is located
    void *return_addr = __builtin_return_address(0);
    uintptr_t addr = (uintptr_t)return_addr;
    
    // Check if executing from Flash (XIP region)
    if (addr >= FLASH_XIP_BASE && addr < FLASH_XIP_END) {
        panic("FATAL: Code is executing from Flash (0x%08x)!\n"
              "       Expected execution from SRAM (0x20000000-0x20042000).\n"
              "       Check CMakeLists.txt: pico_set_binary_type(target copy_to_ram)\n",
              addr);
    }
    
    // Check if executing from SRAM (expected)
    if (addr >= SRAM_BASE && addr < SRAM_END) {
        // Success - code is in SRAM
        return;
    }
    
    // Unexpected memory region
    panic("WARNING: Code executing from unexpected memory region (0x%08x)!\n"
          "         Expected SRAM: 0x20000000-0x20042000\n"
          "         Got: 0x%08x\n",
          addr, addr);
}

#else

// Stub implementation when RUN_FROM_RAM_CHECK is not defined
// Compiler will optimize this away entirely (zero overhead)
void ram_check_verify(void) {
    // No-op in release builds
}

#endif // RUN_FROM_RAM_CHECK
