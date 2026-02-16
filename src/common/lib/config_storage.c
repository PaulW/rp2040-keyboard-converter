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

#include "config_storage.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/platform.h"

#include "config.h"
#include "keymaps.h"
#include "log.h"

// --- Keyboard Identification ---

/**
 * @brief Generate keyboard ID from compile-time defines
 * 
 * Creates a hash from keyboard make, model, protocol, and codeset to uniquely
 * identify the keyboard configuration. Used for smart persistence: if the
 * keyboard ID changes (different firmware), shift-override resets to disabled.
 * 
 * Simple FNV-1a hash algorithm (32-bit):
 * - Fast, good distribution, no crypto needed
 * - Hash at runtime from compile-time string defines
 * 
 * @return 32-bit keyboard identifier hash
 */
static inline uint32_t get_keyboard_id(void) {
    // Keyboard defines from CMake (passed as compile-time strings)
#ifdef _KEYBOARD_ENABLED
    const char *make = _KEYBOARD_MAKE;
    const char *model = _KEYBOARD_MODEL;
    const char *protocol = _KEYBOARD_PROTOCOL;
    const char *codeset = _KEYBOARD_CODESET;
    
    // FNV-1a hash constants
    uint32_t hash = 0x811c9dc5;  // FNV offset basis
    const uint32_t prime = 0x01000193;  // FNV prime
    
    // Hash all four identifier strings
    const char *strings[] = {make, model, protocol, codeset};
    for (int i = 0; i < 4; i++) {
        const char *str = strings[i];
        while (*str) {
            hash ^= (uint32_t)(*str++);
            hash *= prime;
        }
        hash ^= 0xFF;  // Separator between strings
        hash *= prime;
    }
    
    return hash;
#else
    return 0;  // No keyboard configured (shouldn't happen)
#endif
}

// --- Factory Default Configuration ---

/**
 * @brief Factory default configuration
 * 
 * All new fields should have their defaults defined here. When adding
 * a new field:
 * 1. Add to config_data_t in config_storage.h
 * 2. Add default value here
 * 3. Increment CONFIG_VERSION_CURRENT
 * 4. Update get_config_size_for_version()
 */
// --- Factory Default Configuration ---

/**
 * @brief Factory default configuration
 * 
 * This configuration is used when:
 * - No valid config found in flash (first boot)
 * - Both config copies are corrupt
 * - Factory reset is requested
 * - Config version upgrade (as base for migration)
 * 
 * Default Values:
 * - log_level: From LOG_LEVEL_DEFAULT in config.h (compile-time)
 * - led_brightness: From CONVERTER_LEDS_BRIGHTNESS in config.h (compile-time)
 * - All other fields: Zero/empty
 * 
 * Version Upgrade Pattern:
 * When new fields are added to config_data_t, they automatically get
 * these default values during size-based migration (see config_init).
 */

// LED brightness compile-time default handling
// If CONVERTER_LEDS is defined, require CONVERTER_LEDS_BRIGHTNESS to be set
// If CONVERTER_LEDS is not defined, provide a safe default (won't be used)
#ifdef CONVERTER_LEDS
#ifndef CONVERTER_LEDS_BRIGHTNESS
#error "CONVERTER_LEDS_BRIGHTNESS must be defined in config.h when CONVERTER_LEDS is enabled"
#endif
#define LED_BRIGHTNESS_DEFAULT CONVERTER_LEDS_BRIGHTNESS
#else
#define LED_BRIGHTNESS_DEFAULT 5  // Safe default when LEDs disabled (unused)
#endif

static const config_data_t DEFAULT_CONFIG = {
    .magic = CONFIG_MAGIC,
    .version = CONFIG_VERSION_CURRENT,
    .crc16 = 0,  // Calculated at save time
    .sequence = 0,
    .log_level = LOG_LEVEL_DEFAULT,  // From config.h
    .led_brightness = LED_BRIGHTNESS_DEFAULT,  // From config.h or default
    .keyboard_id = 0,  // Set dynamically at init (from get_keyboard_id())
    .flags = { .dirty = 0, .shift_override_enabled = 0, .reserved = 0 },
    .layer_state = 0x01,  // Layer 0 always active (bit 0 set)
    .layers_hash = 0,  // Set dynamically at init (from keymap_compute_layers_hash())
    .reserved = {0},
    .storage = {0}
};

// --- Global State ---

/** RAM-resident configuration (loaded at boot, accessed directly) */
static config_data_t g_config;

/** Initialization flag */
static bool g_initialized = false;

// --- CRC-16/CCITT Implementation ---

/**
 * @brief Update CRC-16/CCITT with new data
 * 
 * Continues a CRC-16/CCITT calculation over additional data. This allows
 * computing a CRC over non-contiguous memory regions while maintaining
 * proper algorithm integrity.
 * 
 * Uses polynomial 0x1021 (x^16 + x^12 + x^5 + 1) for error detection.
 * This is a standard CRC used in many protocols.
 * 
 * @param crc Current CRC value (use 0xFFFF to start)
 * @param data Pointer to data
 * @param length Number of bytes
 * @return Updated 16-bit CRC value
 */
static uint16_t crc16_update(uint16_t crc, const void *data, size_t length) {
    const uint8_t *bytes = (const uint8_t *)data;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Calculate CRC for config structure
 * 
 * CRC covers everything except the crc16 field itself. This is computed
 * as a continuous CRC over two memory regions (before and after the crc16
 * field) to properly detect multi-bit corruptions.
 * 
 * @param cfg Config structure
 * @return 16-bit CRC value
 */
static uint16_t config_calculate_crc(const config_data_t *cfg) {
    // Calculate offsets for the two regions around crc16 field
    size_t before_crc = offsetof(config_data_t, crc16);
    size_t after_crc = before_crc + sizeof(cfg->crc16);
    size_t tail_len = sizeof(config_data_t) - after_crc;
    
    // Compute continuous CRC over both regions
    uint16_t crc = 0xFFFF;  // Start with standard seed
    crc = crc16_update(crc, cfg, before_crc);  // Hash everything before crc16
    crc = crc16_update(crc, (const uint8_t *)cfg + after_crc, tail_len);  // Continue with everything after crc16
    
    return crc;
}

// --- Flash Access ---

/**
 * @brief Read config from flash
 * 
 * @param copy_index 0 for copy A, 1 for copy B
 * @return Pointer to flash memory (XIP-mapped, direct access)
 */
static const config_data_t* read_flash_config(uint8_t copy_index) {
    uint32_t offset = CONFIG_FLASH_OFFSET + (copy_index * CONFIG_COPY_SIZE);
    return (const config_data_t*)(XIP_BASE + offset);
}

/**
 * @brief Validate config structure
 * 
 * Checks magic number and CRC for integrity.
 * 
 * @param cfg Config to validate
 * @return true if valid, false if corrupt
 */
static bool validate_config(const config_data_t *cfg) {
    // Check magic number
    if (cfg->magic != CONFIG_MAGIC) {
        return false;
    }
    
    // Check CRC
    uint16_t calculated_crc = config_calculate_crc(cfg);
    if (cfg->crc16 != calculated_crc) {
        return false;
    }
    
    return true;
}

/**
 * @brief Get config size for a specific version
 * 
 * Returns the size of the config structure for a given version.
 * This enables size-based migration without explicit conversion functions.
 * 
 * **When adding fields:**
 * Add new case here with offsetof() to the FIRST NEW FIELD in that version.
 * 
 * Example:
 * ```
 * Version 1: Has log_level                           → size = offsetof(storage)
 * Version 2: Adds led_brightness before storage      → size = offsetof(storage)
 * Version 3: Adds macro_count before storage         → size = sizeof(config_data_t)
 * ```
 * 
 * @param version Config version number
 * @return Size in bytes, or 0 if unknown version
 */
static size_t get_config_size_for_version(uint16_t version) {
    switch (version) {
        case 1:
            // v1: log_level, led_brightness, flags (no shift_override_enabled bit)
            // Size up to (but not including) keyboard_id field
            return offsetof(config_data_t, keyboard_id);
            
        case 2:
            // v2: Added keyboard_id, shift_override_enabled flag
            // Size up to (but not including) layer_state field
            return offsetof(config_data_t, layer_state);
            
        case 3:
            // v3: Added layer_state, layers_hash for toggle layer persistence
            return sizeof(config_data_t);
        
        default:
            LOG_ERROR("Unknown config version: %d\n", version);
            return 0;  // Unknown version
    }
}

/**
 * @brief Initialize config with factory defaults
 * 
 * @param cfg Config structure to initialize
 */
static void init_factory_defaults(config_data_t *cfg) {
    memcpy(cfg, &DEFAULT_CONFIG, sizeof(config_data_t));
    cfg->keyboard_id = get_keyboard_id();  // Set dynamically at runtime
    cfg->flags.dirty = 1;  // Mark as needing save
}

// --- Public API Implementation ---

bool config_init(void) {
    if (g_initialized) {
        LOG_WARN("Config already initialized\n");
        return true;
    }
    
    LOG_INFO("Initializing configuration system...\n");
    
    // Read both copies from flash
    const config_data_t *copy_a = read_flash_config(0);
    const config_data_t *copy_b = read_flash_config(1);
    
    bool a_valid = validate_config(copy_a);
    bool b_valid = validate_config(copy_b);
    
    const config_data_t *source = NULL;
    
    if (a_valid && b_valid) {
        // Both valid - use newest (highest sequence number)
        source = (copy_a->sequence > copy_b->sequence) ? copy_a : copy_b;
        LOG_INFO("Config loaded: Using copy %s (seq=%u)\n",
                 (source == copy_a) ? "A" : "B", (unsigned int)source->sequence);
    } else if (a_valid) {
        source = copy_a;
        LOG_WARN("Config loaded: Copy B corrupt, using copy A (seq=%u)\n", (unsigned int)source->sequence);
    } else if (b_valid) {
        source = copy_b;
        LOG_WARN("Config loaded: Copy A corrupt, using copy B (seq=%u)\n", (unsigned int)source->sequence);
    } else {
        // Both corrupt - use factory defaults
        LOG_WARN("Config corrupt: Using factory defaults\n");
        init_factory_defaults(&g_config);
        g_initialized = true;
        config_save();  // Save fresh config
        return false;
    }
    
    // Start with factory defaults
    init_factory_defaults(&g_config);
    
    // Overlay data from flash (size-based migration)
    if (source->version <= CONFIG_VERSION_CURRENT) {
        size_t copy_size = get_config_size_for_version(source->version);
        
        if (copy_size > 0 && copy_size <= sizeof(config_data_t)) {
            // Copy whatever fits from old version
            memcpy(&g_config, source, copy_size);
            
            // Update version if upgraded
            if (source->version < CONFIG_VERSION_CURRENT) {
                LOG_INFO("Config upgraded: v%d → v%d\n", 
                         source->version, CONFIG_VERSION_CURRENT);
                g_config.version = CONFIG_VERSION_CURRENT;
                g_config.flags.dirty = 1;  // Mark for save
            } else {
                g_config.flags.dirty = 0;  // In sync with flash
            }
        } else {
            LOG_ERROR("Invalid config size for v%d, using defaults\n", source->version);
            init_factory_defaults(&g_config);
            g_initialized = true;
            config_save();
            return false;
        }
    } else {
        // Config from future version - use defaults with warning
        LOG_ERROR("Config from future version %d (current: %d), using defaults\n",
                  source->version, CONFIG_VERSION_CURRENT);
        init_factory_defaults(&g_config);
        g_initialized = true;
        config_save();
        return false;
    }
    
    g_initialized = true;
    
    // Smart persistence: Check if keyboard configuration changed
    uint32_t current_keyboard_id = get_keyboard_id();
    if (g_config.keyboard_id != current_keyboard_id) {
        LOG_INFO("Keyboard config changed (0x%08X → 0x%08X)\n",
                 (unsigned int)g_config.keyboard_id, (unsigned int)current_keyboard_id);
        LOG_INFO("Resetting shift-override and layer state to defaults\n");
        g_config.keyboard_id = current_keyboard_id;
        g_config.flags.shift_override_enabled = 0;  // Reset to disabled
        g_config.layer_state = 0x01;  // Reset to Layer 0 only
        g_config.layers_hash = 0;  // Will be recomputed by keymap_init()
        g_config.flags.dirty = 1;  // Mark for save
    }
    
    // Sanity check: Validate shift-override availability
    // If config says enabled but keyboard doesn't define shift-override arrays, disable it
    extern const uint8_t * const keymap_shift_override_layers[KEYMAP_MAX_LAYERS] __attribute__((weak));
    
    if (g_config.flags.shift_override_enabled && keymap_shift_override_layers == NULL) {
        LOG_WARN("Shift-override enabled in config but keyboard doesn't define shift mappings\n");
        LOG_INFO("Disabling shift-override (keymap_shift_override_layers not defined)\n");
        g_config.flags.shift_override_enabled = 0;
        g_config.flags.dirty = 1;  // Mark for save
    }
    
    // Save if upgraded or keyboard changed
    if (g_config.flags.dirty) {
        config_save();
    }
    
    return true;
}

const config_data_t* config_get(void) {
    if (!g_initialized) {
        LOG_ERROR("Config not initialized!\n");
        // Return pointer anyway (will be zeros if not init)
    }
    return &g_config;
}

void config_set_log_level(uint8_t level) {
    if (!g_initialized) {
        LOG_ERROR("Config not initialized!\n");
        return;
    }
    
    if (g_config.log_level != level) {
        g_config.log_level = level;
        g_config.flags.dirty = 1;
        LOG_DEBUG("Log level changed to %d (pending save)\n", level);
    }
}

void config_set_led_brightness(uint8_t level) {
    if (!g_initialized) {
        LOG_ERROR("Config not initialized!\n");
        return;
    }
    
    // Clamp to valid range [0-10]
    if (level > 10) {
        level = 10;
    }
    
    if (g_config.led_brightness != level) {
        g_config.led_brightness = level;
        g_config.flags.dirty = 1;
        LOG_DEBUG("LED brightness changed to %d (pending save)\n", level);
    }
}

uint8_t config_get_led_brightness(void) {
    if (!g_initialized) {
        LOG_WARN("Config not initialized, returning default brightness\n");
        return LED_BRIGHTNESS_DEFAULT;  // Return compile-time default
    }
    
    return g_config.led_brightness;
}

void config_set_shift_override_enabled(bool enabled) {
    if (!g_initialized) {
        LOG_ERROR("Config not initialized!\n");
        return;
    }
    
    bool current = (g_config.flags.shift_override_enabled != 0);
    if (current != enabled) {
        g_config.flags.shift_override_enabled = enabled ? 1 : 0;
        g_config.flags.dirty = 1;
        LOG_DEBUG("Shift-override %s (pending save)\n", enabled ? "enabled" : "disabled");
    }
}

bool config_get_shift_override_enabled(void) {
    if (!g_initialized) {
        LOG_WARN("Config not initialized, shift-override disabled\n");
        return false;  // Default to disabled
    }
    
    return (g_config.flags.shift_override_enabled != 0);
}

bool config_save(void) {
    if (!g_initialized) {
        LOG_ERROR("Config not initialized!\n");
        return false;
    }
    
    if (!g_config.flags.dirty) {
        // No changes - skip write
        return true;
    }
    
    LOG_DEBUG("Saving config to flash...\n");
    
    // Prepare a clean copy for writing (dirty flag must be 0 in persisted copy)
    config_data_t write_config = g_config;
    write_config.flags.dirty = 0;
    write_config.sequence++;
    write_config.crc16 = config_calculate_crc(&write_config);
    
    // Determine which copy to write (alternating A/B for wear leveling)
    uint8_t new_copy_index = (write_config.sequence & 1);
    uint8_t old_copy_index = 1 - new_copy_index;
    
    LOG_DEBUG("Writing to copy %c (seq=%u)\n", 
             new_copy_index ? 'B' : 'A', (unsigned int)write_config.sequence);
    
    // Prepare buffer with both copies (4KB sector)
    // Must preserve both copies because erase wipes entire sector
    static uint8_t sector_buffer[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));
    memset(sector_buffer, 0xFF, FLASH_SECTOR_SIZE);  // Erased flash = 0xFF
    
    // Read old copy from flash (to preserve after erase)
    const config_data_t *old_copy = read_flash_config(old_copy_index);
    
    // Copy old config to buffer (if valid, otherwise use current config with seq-1)
    if (validate_config(old_copy)) {
        memcpy(sector_buffer + (old_copy_index * CONFIG_COPY_SIZE), 
               old_copy, sizeof(config_data_t));
    } else {
        // Old copy invalid, write current config with decremented sequence as backup
        config_data_t backup = write_config;
        backup.sequence--;
        backup.crc16 = config_calculate_crc(&backup);
        memcpy(sector_buffer + (old_copy_index * CONFIG_COPY_SIZE), 
               &backup, sizeof(config_data_t));
    }
    
    // Copy new config to buffer
    memcpy(sector_buffer + (new_copy_index * CONFIG_COPY_SIZE), 
           &write_config, sizeof(config_data_t));
    
    // Flash write requires interrupts disabled
    uint32_t ints = save_and_disable_interrupts();
    
    // Erase entire 4KB sector
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    
    // Write entire sector back (both copies)
    flash_range_program(CONFIG_FLASH_OFFSET, sector_buffer, FLASH_SECTOR_SIZE);
    
    // Re-enable interrupts
    restore_interrupts(ints);
    
    // Update RAM copy with the clean version that was written
    g_config = write_config;
    
    LOG_DEBUG("Config saved successfully\n");
    return true;
}

void config_factory_reset(void) {
    LOG_WARN("Factory reset: Restoring defaults\n");
    
    // Initialize with factory defaults (sequence starts at 0)
    init_factory_defaults(&g_config);
    g_initialized = true;
    
    // Erase entire config sector to clear both copies
    // This ensures a true "factory reset" with no old data
    LOG_INFO("Erasing config flash sector...\n");
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    
    // Now save fresh config (sequence will be incremented to 1)
    config_save();
}

// --- Layer State Persistence (v3) ---

void config_set_layer_state(uint8_t layer_state) {
    if (!g_initialized) {
        LOG_ERROR("Config not initialized!\n");
        return;
    }
    
    if (g_config.layer_state != layer_state) {
        g_config.layer_state = layer_state;
        g_config.flags.dirty = 1;
        LOG_DEBUG("Layer state changed to 0x%02X (pending save)\n", layer_state);
    }
}

uint8_t config_get_layer_state(void) {
    if (!g_initialized) {
        LOG_WARN("Config not initialized, returning Layer 0 only\n");
        return 0x01;  // Only Layer 0 active
    }
    
    // Always return Layer 0 active (bit 0 set)
    return g_config.layer_state | 0x01;
}

void config_set_layers_hash(uint32_t layers_hash) {
    if (!g_initialized) {
        LOG_ERROR("Config not initialized!\n");
        return;
    }
    
    if (g_config.layers_hash != layers_hash) {
        g_config.layers_hash = layers_hash;
        g_config.flags.dirty = 1;
        LOG_DEBUG("Layers hash changed to 0x%08X (pending save)\n", 
                 (unsigned int)layers_hash);
    }
}

uint32_t config_get_layers_hash(void) {
    if (!g_initialized) {
        LOG_WARN("Config not initialized, returning 0\n");
        return 0;
    }
    
    return g_config.layers_hash;
}
