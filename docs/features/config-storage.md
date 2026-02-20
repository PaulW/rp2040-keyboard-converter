# Configuration Storage

The converter needs to remember certain settings between power cycles—things like your preferred log level or LED brightness. This page explains how those settings are stored reliably in the RP2040's internal flash memory, and why the system is designed to protect your configuration even if power is lost during a write operation.

---

## Why Persistent Storage Matters

When you adjust the log level through Command Mode or change LED brightness, you expect those settings to "stick." If they reset to defaults every time you unplug the converter, you'd have to reconfigure them constantly—annoying and impractical.

The RP2040 doesn't have dedicated EEPROM like some microcontrollers, but it does have 2MB of internal flash memory. Flash memory stores the firmware code and constant data, with a small portion reserved for configuration storage. Flash memory retains data without power, making it suitable for persistent settings.

However, flash has some quirks we need to handle carefully. Flash cells wear out after many write cycles (manufacturer specifications vary by chip). Flash must be erased in relatively large blocks (4KB sectors on the RP2040) before writing new data. And if power is lost partway through a write operation, the data might end up corrupted.

The configuration storage system addresses all these concerns whilst keeping the implementation simple and robust.

---

## What Gets Stored

The converter persists several settings across reboots:

**Log Level** - Controls how verbose the UART debug output is. It can be ERROR (minimal), INFO (moderate, default), or DEBUG (verbose). You set this through Command Mode using the 'D' command.

**LED Brightness** - Controls how bright the status LED and any WS2812 RGB LEDs appear, on a scale from 0 (off) to 10 (maximum). You set this through Command Mode using the 'L' command.

**Shift-Override Enable** - Runtime toggle for keyboards with non-standard shift legends (terminal keyboards, vintage layouts). Disabled by default, enabled via Command Mode 'S' command. Only affects keyboards that define shift-override arrays.

**Layer State** - Active toggle (TG) and permanent switch (TO) layers. When you toggle Dvorak on or switch to a gaming layer, that state persists across reboots. Momentary (MO) and one-shot (OSL) layers don't persist—they're temporary by design.

These settings are stored together in a single configuration structure, along with metadata like a version number, sequence counter, and CRC checksum. The configuration structure occupies 2048 bytes total (matching the 2KB copy size), with 26 bytes of header and settings (version, sequence, magic, CRC, log level, LED brightness, keyboard ID, layer state, layers hash, flags, and reserved padding), and 2022 bytes reserved for variable storage (TLV storage for macros, key remaps, etc.).

Future firmware versions might add more settings—custom key remapping, macro definitions, debounce timing adjustments, protocol-specific tweaks. The storage system is designed to accommodate this growth through automatic version migration.

---

## Flash Memory Allocation

The RP2040 has 2MB of internal flash memory starting at address 0x10000000. The firmware occupies the beginning of this space—code, constant data, strings, everything that makes the converter work. The exact size varies depending on which features are enabled.

Configuration storage lives at the very end of flash memory, in the last 4KB. This fixed location approach means configuration never overlaps with firmware, regardless of how large the firmware grows. The last 4KB is divided into two 2KB sections, called Copy A and Copy B.

Here's the layout:

```
0x10000000 ──┬─── Firmware start
             │
             │    Firmware code and data
             │    (~80-120KB, varies by configuration)
             │
0x101FF000 ──├─── Configuration Copy A (2KB)
0x101FF800 ──├─── Configuration Copy B (2KB)
0x10200000 ──└─── Flash end (2MB total)
```

Why use the last 4KB? Several reasons:

**Fixed location**: Firmware size can change between versions or configurations, but the last 4KB is always at the same address. Configuration code doesn't need to know anything about firmware size.

**No collision risk**: Since firmware grows from the beginning and configuration lives at the end, they can't accidentally overlap.

**Aligned to sector boundary**: The RP2040 erases flash in 4KB sectors. Placing configuration at a sector boundary means erasing it doesn't affect firmware.

**Minimal waste**: Using 4KB out of 2MB (0.2%) is negligible. We get plenty of room for future settings whilst barely impacting available firmware space.

---

## Dual-Copy Redundancy

The reason configuration storage uses two 2KB sections instead of one becomes clear when you consider what happens during a write operation.

### The Problem with Single-Copy Storage

Imagine we had only one copy of configuration in flash. When you change a setting:

1. Erase the flash sector (now the configuration is gone)
2. Write the new configuration data

If power fails after step 1 but before step 2 completes, your configuration is lost. Even worse, you're left with either garbage data or an incomplete write that might confuse the firmware on the next boot.

### How Dual-Copy Solves This

With two copies, the system maintains redundancy throughout the write process. The basic idea: never erase the only good copy until you've successfully written a new one.

Here's how a configuration update works:

1. Read both copies (A and B) from flash
2. Determine which copy is newer based on sequence numbers
3. Write new configuration to the **other** copy
4. Verify the write succeeded
5. Only now is it safe to erase the old copy (though the implementation leaves it as backup)

At any point in this process, at least one valid copy exists. Power failure can't corrupt both copies simultaneously.

### Sequence Numbers Track Freshness

Each configuration copy includes a sequence number that increments with every write. Copy A might have sequence 47, whilst Copy B has sequence 48. The system knows Copy B is newer.

When writing new configuration, the sequence number increments again—Copy B (48) gets updated to sequence 49, or if we're alternating, Copy A gets updated from 47 to 49.

On boot, the system reads both copies, checks their CRCs to verify integrity, and uses whichever copy has the higher sequence number. If one copy is corrupted (CRC mismatch), the other copy is used. If both copies are somehow corrupted (extremely rare), the system falls back to hardcoded defaults.

---

## Wear Leveling Through Alternation

Flash memory has finite write endurance—manufacturer specifications vary per sector before cells start wearing out. If we always wrote to the same location, we'd wear out that sector whilst the rest of flash remains pristine.

The dual-copy design provides natural wear leveling by alternating writes between Copy A and Copy B. When you save configuration:

- If Copy A is currently newer (higher sequence), write to Copy B
- If Copy B is currently newer (higher sequence), write to Copy A
- This alternation distributes writes evenly across both 2KB sectors

If you change settings 10,000 times, each copy gets written 5,000 times instead of one copy getting all 10,000 writes. This doubles the effective lifetime of the configuration storage.

The alternation strategy distributes wear across multiple sectors, extending the practical lifetime of the configuration storage.

---

## CRC Validation

Each configuration copy includes a CRC-16/CCITT checksum computed over all the configuration data. This checksum serves two purposes: detecting corruption from power loss or flash wear, and quickly identifying which copy is valid.

CRC (Cyclic Redundancy Check) is a mathematical hash that produces a 16-bit value for any chunk of data. If even a single bit changes in the data, the CRC will change as well. By storing the CRC alongside the configuration and recomputing it when reading, we can detect any corruption.

When the system boots:

1. Read Copy A from flash
2. Compute CRC over Copy A's data
3. Compare computed CRC to stored CRC
4. If they match, Copy A is valid
5. Repeat for Copy B
6. Use the valid copy with the highest sequence number

If a copy's CRC doesn't match, that copy is ignored. If both copies have invalid CRCs, the system uses factory defaults and writes a fresh configuration to flash.

The CRC-16/CCITT algorithm is fast (a few microseconds for our small configuration structure) and provides excellent error detection for the kinds of corruption we might see from power loss or flash wear.

---

## Boot Process

When the converter powers on, one of the first things it does is load configuration from flash. This happens before the main event loop starts, ensuring all subsystems have correct configuration from the beginning.

Here's the complete boot sequence:

**Step 1**: Initialise the configuration system. This sets up pointers to the flash storage locations and prepares the runtime configuration structure in RAM.

**Step 2**: Read Copy A from flash (2KB from 0x101FF000).

**Step 3**: Validate Copy A by computing its CRC and comparing to the stored CRC. Also check that its version number is recognised.

**Step 4**: Read Copy B from flash (2KB from 0x101FF800).

**Step 5**: Validate Copy B the same way.

**Step 6**: Determine which copy to use:
- If both valid: Use the copy with the higher sequence number (more recent)
- If only one valid: Use that one
- If neither valid: Use factory defaults

**Step 7**: Copy the selected configuration data to RAM, where it lives for the rest of the session.

**Step 8**: If factory defaults were used (both copies invalid), write them to flash immediately so valid configuration exists for next boot.

This process is fast enough to be imperceptible during boot. The converter is ready to use with correct configuration applied.

---

## Runtime Access

Once configuration is loaded at boot, all settings live in a structure in RAM. Code throughout the firmware accesses configuration via the API:

```c
// Access settings via API - zero overhead (inline pointer return)
const config_data_t *cfg = config_get();
uint8_t current_level = cfg->log_level;
uint8_t brightness = cfg->led_brightness;
```

This design has zero runtime performance cost. Accessing configuration is just a RAM read—no function calls, no indirection, no overhead. The compiler can even optimise these reads into registers when possible.

The tradeoff is that changes to configuration don't automatically save. When you modify a setting through Command Mode, the code updates the RAM structure and then explicitly calls the save function. This explicit save design gives you control over when the write operation happens.

---

## Saving Configuration

When you change a setting through Command Mode, the new value gets written to the RAM configuration structure immediately. However, this change only exists in RAM—if power were lost right then, the change would be gone on next boot.

To persist the change, the system must write the configuration to flash. This happens automatically after you confirm a Command Mode operation, but it's worth understanding what occurs during that write.

### Save Process

**Step 1**: Mark the configuration structure with updated timestamp and increment the sequence number.

**Step 2**: Compute a new CRC over all the configuration data.

**Step 3**: Determine which copy (A or B) to write based on which is currently older. This implements the wear leveling alternation.

**Step 4**: Disable interrupts and erase the entire 4KB flash sector.

**Step 5**: Write both configuration copies to the erased sector.

**Step 6**: Re-enable interrupts and mark the operation complete.

The entire save operation is a blocking flash write—fast enough to feel instant, but slow enough that we don't want it happening frequently. That's why writes are explicit rather than automatic on every setting change.

### Blocking vs Non-Blocking

The save operation blocks the main loop and **disables interrupts** whilst writing to flash. During this time, the converter cannot process IRQ events, including keyboard scancodes or USB communication.

However, data loss is prevented because the **PIO hardware continues operating independently** with its own RX FIFO buffer (4-8 entries deep). Scancodes captured by the PIO are held in this hardware FIFO. When interrupts are re-enabled after the flash write completes, the IRQ handler immediately processes any buffered data and transfers it to the 32-byte ring buffer for normal processing.

For USB, the host polls every 8 milliseconds (configured by `bInterval` in [`usb_descriptors.c`](../../src/common/lib/usb_descriptors.c)). The flash write spans a few poll cycles where the host sees the converter as "no keys pressed", then normal operation resumes. Unless you're actively typing the instant you trigger a save command (which is unlikely—you just finished using Command Mode), this pause is imperceptible.

The alternative—a non-blocking async save—would add significant complexity for minimal benefit. The current blocking design is simple, reliable, and fast enough.

---

## Version Migration

As firmware evolves, the configuration structure might gain new fields. For example, a future version might add a `key_debounce_ms` setting. How do we handle this without breaking existing installations?

The configuration structure includes a version number field. The current version is 3:
- **v1**: Added log_level and led_brightness
- **v2**: Added keyboard_id (for smart persistence) and shift_override_enabled flag
- **v3**: Added layer_state and layers_hash (for layer persistence with validation)

When the firmware boots, it checks the configuration version:

- **Version matches**: Configuration is current, use it as-is
- **Version older**: Run migration code to add new fields with defaults
- **Version newer**: Firmware is older than the configuration (shouldn't happen, but handle gracefully by using defaults)

### Size-Based Migration

The current implementation uses a simple migration strategy: if the configuration size in flash is smaller than the current configuration structure size, it's from an older version.

When migrating:
1. Read the old configuration into a temporary buffer
2. Create a new configuration structure initialised to defaults
3. Copy all fields that exist in both old and new versions
4. New fields retain their default values
5. Write the migrated configuration to flash with updated version number

This approach preserves user settings whilst adding new features with sensible defaults.

### Example Migration

This is a simplified hypothetical example to illustrate the migration process. Suppose Version 1 has `log_level` and `led_brightness`. Version 2 adds `key_debounce_ms`:

1. Boot with Version 2 firmware
2. Read configuration from flash, find it's Version 1 (smaller size)
3. Create new Version 2 structure with `key_debounce_ms = 5` (default)
4. Copy `log_level` and `led_brightness` from old configuration
5. Write migrated Version 2 configuration to flash
6. From now on, boot with Version 2 configuration

The user's log level and brightness settings are preserved, and the new debounce setting starts at a sensible default they can adjust if needed.

(Note: The actual v1→v2→v3 migration history is documented above, but this example uses simplified field names for clarity.)

---

## Factory Reset

The Command Mode 'F' command (implemented in [`command_mode.c`](../../src/common/lib/command_mode.c)) performs a factory reset, which wipes all saved configuration and restores defaults. After the reset completes, the converter automatically reboots to apply the factory defaults. This is implemented by:

1. Initialise configuration structure to factory defaults (INFO log level, brightness 5)
2. Set version number to current version
3. Set sequence number to 0
4. Write to both Copy A and Copy B
5. Mark both copies as valid
6. Trigger a watchdog reboot (cleanest reboot method)

After factory reset, both copies contain identical factory-default configuration with sequence 0. The next save will increment sequence to 1 and alternate between copies as normal.

Factory reset is useful when you've experimented with settings and want a clean start, or when handing the converter to someone else and want to clear your customisations.

---

## Implementation Details

The configuration storage code lives in [`config_storage.c`](../../src/common/lib/config_storage.c) and [`config_storage.h`](../../src/common/lib/config_storage.h). It provides a simple API:

```c
// Boot: Load configuration from flash to RAM
void config_init(void);

// Runtime: Save RAM configuration to flash
bool config_save(void);

// Runtime: Reset to factory defaults and save
bool config_factory_reset(void);

// Runtime: Access settings
extern config_t g_config;
```

The implementation uses the Pico SDK's flash APIs ([`flash_range_erase`](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#hardware_flash) and [`flash_range_program`](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#hardware_flash)) which are wrappers around the RP2040's boot ROM flash functions. These boot ROM functions are thoroughly tested and reliable.

One quirk: you can't execute code from flash whilst writing to flash. The RP2040 bootloader handles this by copying the flash write functions to RAM before calling them. The Pico SDK's flash APIs do this automatically, so the configuration storage code doesn't need special handling.

---

## Performance Characteristics

**Boot time overhead**: Negligible—reads and validates configuration from flash once at startup.

**Runtime overhead**: Zero. Direct RAM access via inline pointer return.

**Save time**: Brief blocking flash write. Ring buffer protects against data loss during save.

**Flash wear**: Each setting change alternates between Copy A and Copy B. With 10,000 cycle endurance, you could save configuration 20,000 times before wearing out flash. At one save per day, that's 54 years.

**Corruption resistance**: Dual-copy with CRC validation protects against power loss during write. Configuration protection remains robust even with frequent power cycling.

---

## Troubleshooting

### Settings Don't Persist

If you change settings through Command Mode but they reset to defaults on the next boot:

**Flash write failure**: Extremely rare, but possible if flash is worn out or there's a hardware problem. Check UART logs for error messages during save operations.

**Wrong firmware**: If you flash a different firmware configuration that doesn't match your saved configuration structure, migration might fail and defaults get used. This is expected when switching between major firmware versions.

**Explicit factory reset**: Double-check you didn't trigger 'F' (factory reset) accidentally during Command Mode.

### Configuration Corrupts After Power Loss

If power loss during a save operation seems to corrupt configuration:

**This shouldn't happen** thanks to dual-copy redundancy. If it does, check UART logs on the next boot—you should see which copy was used and why.

**One copy corrupted is expected and handled**: Power loss during write might corrupt one copy, but the other copy should remain valid. The system should use the valid copy automatically.

**Both copies corrupted**: This requires power failure during the write operation that affects both copies. If this happens, the system uses factory defaults and you'll need to reconfigure.

### Unexpected Defaults After Firmware Update

If settings reset to defaults after flashing new firmware:

**Check if configuration structure changed**: If new firmware added or removed settings, migration might have issues. This is expected and you can reconfigure through Command Mode.

**Verify you flashed compatible firmware**: Switching between keyboard configurations (e.g., AT/PS2 to XT) doesn't change configuration storage, but switching between fundamentally different builds might.

---

## Related Documentation

**Features that use configuration storage**:
- [Command Mode](command-mode.md) - User interface for changing settings
- [Logging](logging.md) - Log level setting stored in configuration
- [LED Support](led-support.md) - LED brightness setting stored in configuration

**Implementation**:
- [`config_storage.c`](../../src/common/lib/config_storage.c) - Storage implementation
- [`config_storage.h`](../../src/common/lib/config_storage.h) - API and data structures

**Background reading**:
- [Pico SDK Flash Documentation](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#hardware_flash) - Low-level flash APIs

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
