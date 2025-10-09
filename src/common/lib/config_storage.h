/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
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
 * @file config_storage.h
 * @brief Flash-based configuration storage with version migration
 * 
 * This module provides persistent configuration storage using the RP2040's
 * internal flash memory. Configuration is stored in the last 4KB sector of
 * flash and loaded into RAM at boot for fast access.
 * 
 * **Architecture:**
 * 
 * 1. **Flash → RAM at Boot:**
 *    - Read both config copies from flash (A and B)
 *    - Validate CRC and magic numbers
 *    - Use newest valid copy (highest sequence number)
 *    - Fall back to factory defaults if both corrupt
 *    - Copy to RAM for zero-overhead runtime access
 * 
 * 2. **Runtime Access:**
 *    - All reads from RAM (no flash access)
 *    - Writes update RAM immediately
 *    - "Dirty" flag tracks pending changes
 * 
 * 3. **RAM → Flash on Save:**
 *    - Only writes if dirty flag set
 *    - Increments sequence number
 *    - Recalculates CRC
 *    - Writes to alternate copy (wear leveling)
 *    - ~25ms blocking operation (acceptable for rare writes)
 * 
 * **Version Migration:**
 * 
 * Uses size-based automatic migration:
 * - Newer firmware reading old config: Missing fields use defaults
 * - Older firmware reading new config: Extra fields ignored (best effort)
 * - No explicit migration functions needed
 * - Just increment CONFIG_VERSION_CURRENT when changing struct
 * 
 * **Dual-Copy Redundancy:**
 * 
 * Two copies (A and B) protect against corruption:
 * - Sequence number determines newest copy
 * - Alternating writes for wear leveling (~10K cycles per location)
 * - Power loss during write: Other copy still valid
 * 
 * **Flash Layout (Last 4KB of 2MB):**
 * ```
 * 0x101FF000 - Config Copy A (2KB)
 * 0x101FF800 - Config Copy B (2KB)
 * ```
 * 
 * **Memory Usage:**
 * - Flash: 4KB (0.2% of 2MB)
 * - RAM: ~2KB (0.7% of 264KB)
 * - Code: ~2KB
 * 
 * **Performance:**
 * - Read: 0 cycles (direct RAM access)
 * - Write RAM: 2-3 cycles
 * - Write Flash: ~25ms (blocking, rare operation)
 * - Boot Load: ~100µs (one-time)
 * 
 * **Thread Safety:**
 * - config_get(): Thread-safe (const pointer to RAM)
 * - config_save(): NOT thread-safe (disables interrupts briefly)
 * - config_set_*(): NOT thread-safe (use from main loop only)
 * 
 * **Example Usage:**
 * 
 * ```c
 * // Boot initialization
 * if (!config_init()) {
 *     LOG_WARN("Using factory defaults\n");
 * }
 * 
 * // Apply saved settings
 * const config_data_t *cfg = config_get();
 * log_set_level(cfg->log_level);
 * 
 * // Runtime read (zero overhead)
 * if (config_get()->log_level >= LOG_LEVEL_DEBUG) {
 *     // Debug code
 * }
 * 
 * // Change setting
 * config_set_log_level(LOG_LEVEL_DEBUG);
 * config_save();  // Persist to flash
 * ```
 * 
 * @note Flash writes are blocking (~25ms) but safe due to ring buffer
 * @note Always call config_init() before using other functions
 * @note Changes are NOT persisted until config_save() is called
 * 
 * @warning Do not call config_save() from interrupt context
 * @warning Flash sector must not overlap with firmware code
 */

#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

// --- Configuration Constants ---

/** Magic number for config validation ("RP20") */
#define CONFIG_MAGIC 0x52503230

/** Current config version - increment when changing config_data_t */
#define CONFIG_VERSION_CURRENT 1

/** Size of variable storage area (for future TLV data) */
#define CONFIG_STORAGE_SIZE 2028  // ~2KB (fits in 2048 byte copy with header)

/** Flash offset for config storage (last 4KB of flash) */
#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - 4096)

/** Size of each config copy (2KB each, 2 copies = 4KB total) */
#define CONFIG_COPY_SIZE 2048

/** Flash sector size (RP2040 flash sectors are 4KB) */
#define FLASH_SECTOR_SIZE 4096

// --- Configuration Data Structure ---

/**
 * @brief Runtime configuration data (RAM-resident)
 * 
 * This structure lives in RAM after boot and is accessed directly with
 * zero overhead. Changes are persisted to flash only when config_save()
 * is called.
 * 
 * **Version History:**
 * - v1: Initial version (log_level only)
 * 
 * When adding fields:
 * 1. Add field to struct
 * 2. Add default value to DEFAULT_CONFIG in config_storage.c
 * 3. Increment CONFIG_VERSION_CURRENT
 * 4. Update get_config_size_for_version() in config_storage.c
 * 
 * Old configs will automatically upgrade with defaults for new fields.
 */
typedef struct __attribute__((packed)) {
    // --- Header (for flash validation) ---
    uint32_t magic;              /**< CONFIG_MAGIC for validation */
    uint16_t version;            /**< CONFIG_VERSION_CURRENT */
    uint16_t crc16;              /**< CRC-16/CCITT for integrity check */
    uint32_t sequence;           /**< Write counter for dual-copy selection */
    
    // --- Runtime Settings (v1+) ---
    uint8_t log_level;           /**< LOG_LEVEL_ERROR/INFO/DEBUG */
    
    // --- Flags ---
    struct {
        uint8_t dirty : 1;       /**< RAM differs from Flash (needs write) */
        uint8_t reserved : 7;    /**< Reserved for future flags */
    } flags;
    
    // --- Padding (alignment and future expansion) ---
    uint8_t reserved[2];         /**< Alignment padding */
    
    // --- Variable Storage (future use for TLV data) ---
    uint8_t storage[CONFIG_STORAGE_SIZE];  /**< Reserved for macros, key remaps, etc */
    
} config_data_t;

// Compile-time size validation
_Static_assert(sizeof(config_data_t) <= CONFIG_COPY_SIZE, 
               "Config structure exceeds copy size");

// --- Public API ---

/**
 * @brief Initialize configuration system
 * 
 * Loads configuration from flash into RAM. This should be called once
 * at boot before using any other config functions.
 * 
 * **Process:**
 * 1. Read both config copies (A and B) from flash
 * 2. Validate magic number and CRC for each
 * 3. Use newest valid copy (highest sequence number)
 * 4. If both corrupt, use factory defaults and save
 * 5. If version mismatch, overlay old data onto defaults and upgrade
 * 
 * **Return Value:**
 * - `true`: Valid config loaded from flash
 * - `false`: Using factory defaults (no valid config found)
 * 
 * @return true if config loaded successfully, false if using defaults
 * 
 * @note Must be called after init_uart_dma() for logging
 * @note Safe to call multiple times (idempotent)
 * 
 * Example:
 * ```c
 * init_uart_dma();
 * if (!config_init()) {
 *     LOG_WARN("No valid config, using defaults\n");
 * }
 * ```
 */
bool config_init(void);

/**
 * @brief Get pointer to runtime configuration
 * 
 * Returns a const pointer to the RAM-resident configuration. This can
 * be used for direct, zero-overhead reads of any config field.
 * 
 * **Performance:** Direct RAM access, 0 CPU cycles overhead
 * 
 * @return Pointer to config structure (never NULL)
 * 
 * @note Pointer is valid for entire program lifetime
 * @note Do not modify returned structure directly (use config_set_* functions)
 * 
 * Example:
 * ```c
 * const config_data_t *cfg = config_get();
 * printf("Log level: %d\n", cfg->log_level);
 * 
 * // Fast inline check (compiler optimizes to constant)
 * if (config_get()->log_level >= LOG_LEVEL_DEBUG) {
 *     LOG_DEBUG("Debug message\n");
 * }
 * ```
 */
const config_data_t* config_get(void);

/**
 * @brief Set log level
 * 
 * Updates the log level in RAM and marks config as dirty (needs save).
 * Change is NOT persisted until config_save() is called.
 * 
 * @param level LOG_LEVEL_ERROR (0), LOG_LEVEL_INFO (1), or LOG_LEVEL_DEBUG (2)
 * 
 * @note Only updates RAM, call config_save() to persist
 * @note Not thread-safe, call from main loop only
 * 
 * Example:
 * ```c
 * config_set_log_level(LOG_LEVEL_DEBUG);
 * log_set_level(LOG_LEVEL_DEBUG);  // Apply immediately
 * config_save();                    // Persist to flash
 * ```
 */
void config_set_log_level(uint8_t level);

/**
 * @brief Save configuration to flash
 * 
 * Writes the current RAM config to flash if the dirty flag is set.
 * This is a blocking operation that takes ~25ms.
 * 
 * **Process:**
 * 1. Check dirty flag (skip if not dirty)
 * 2. Increment sequence number
 * 3. Recalculate CRC
 * 4. Determine write location (alternating A/B for wear leveling)
 * 5. Erase flash sector (~20ms, blocking)
 * 6. Write config to flash (~5ms, blocking)
 * 7. Clear dirty flag
 * 
 * **Blocking Time:** ~25ms (acceptable due to ring buffer protection)
 * 
 * @return true if write successful or no write needed, false on error
 * 
 * @note Safe to call frequently - only writes if dirty
 * @note Not thread-safe, call from main loop only
 * @note Keyboard input protected by ring buffer during write
 * 
 * @warning Do not call from interrupt context
 * @warning Disables interrupts briefly (~25ms)
 * 
 * Example:
 * ```c
 * // Safe - only writes if changed
 * config_set_log_level(LOG_LEVEL_DEBUG);
 * config_save();  // ~25ms if dirty, ~1µs if not
 * 
 * // Command mode exit - flush all changes
 * config_save();
 * ```
 */
bool config_save(void);

/**
 * @brief Reset configuration to factory defaults
 * 
 * Resets all configuration to compile-time defaults and immediately
 * saves to flash. This is useful for troubleshooting or first-time setup.
 * 
 * **Defaults:**
 * - log_level: LOG_LEVEL_DEFAULT (from config.h)
 * - All other fields: Zero/disabled
 * 
 * **Command Mode Integration:**
 * Could be mapped to 'R' key in command mode for user-accessible reset.
 * 
 * @note Immediately writes to flash (~25ms blocking)
 * @note Not thread-safe, call from main loop only
 * 
 * Example:
 * ```c
 * // Factory reset (e.g., command mode 'R' key)
 * LOG_WARN("Resetting to factory defaults\n");
 * config_factory_reset();
 * log_set_level(config_get()->log_level);  // Apply default
 * ```
 */
void config_factory_reset(void);

#endif /* CONFIG_STORAGE_H */
