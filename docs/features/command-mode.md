# Command Mode

Command Mode provides a keyboard-driven interface for managing the converter's firmware and configuration without needing to unplug it or press physical buttons. This special mode lets you enter the bootloader for firmware updates, adjust debug logging verbosity, reset configuration to defaults, or change LED brightness—all through specific key combinations.

---

## What Command Mode Does

Once you have the converter up and running, you might need to update firmware, troubleshoot issues, or adjust settings. Normally, this would require physical access to the RP2040 board—holding down the BOOTSEL button whilst plugging it in, or accessing configuration files. Command Mode eliminates this hassle by letting you trigger these actions directly from your keyboard.

The converter supports five main operations through Command Mode:

**Bootloader Entry** - Reset into firmware update mode without touching the board. The RP2040 appears as a USB mass storage device where you can drag-and-drop new firmware.

**Log Level Adjustment** - Change how verbose the debug output is on the UART port. Switch between minimal error logging, moderate informational output, or full debug verbosity without recompiling or reflashing. See [Logging](logging.md) for details.

**Factory Reset** - Wipe all saved settings (log level, LED brightness, shift-override state) and restore factory defaults. Useful if you've made configuration changes and want a clean slate. See [Configuration Storage](config-storage.md) for persistence details.

**LED Brightness Control** - Adjust how bright the converter's status LED appears, from off to full brightness. Particularly useful if you have WS2812 RGB LEDs that are too bright in a dark room. See [LED Support](led-support.md) for LED configuration.

**Shift-Override Toggle** - Enable or disable shift-override for keyboards with non-standard shift legends. Only affects keyboards that define custom shift mappings (like terminal keyboards with unusual number row legends).

All these operations complete without interrupting your work—you trigger them through key combinations, wait a moment for confirmation, and continue typing.

---

## Activating Command Mode

The default activation method works on most keyboards: hold down both shift keys simultaneously for three seconds. This combination is unlikely to happen accidentally during normal typing, but easy to remember and execute when needed.

### The Activation Sequence

Here's exactly what happens when you activate Command Mode:

**Step 1**: Press and hold both **Left Shift** and **Right Shift** at the same time.

**Step 2**: Keep holding both keys. After three seconds, you'll see visual confirmation that Command Mode has activated—the status LED starts alternating between green and blue every 100 milliseconds.

**Step 3**: Once you see the alternating LED pattern, you can release both shift keys. Command Mode is now active.

**Step 4**: You have three seconds to press one of the command keys (B, D, F, L, or S). If you don't press anything within three seconds, the converter automatically exits Command Mode and returns to normal operation.

The three-second hold requirement prevents accidental activation. It's long enough that you won't trigger it during normal typing, but short enough that it's not tedious when you actually want to use it.

### Visual and UART Feedback

Command Mode provides two types of feedback to help you understand what's happening:

**LED Visual Feedback** (if WS2812 LEDs are configured):

**Before 3 seconds**: LED shows normal status (green for ready, other colors if there's an issue). Hold both shifts and wait.

**After 3 seconds**: LED alternates green/blue rapidly. This is your signal that Command Mode is active and waiting for a command key.

**After command key press**: LED changes to indicate which operation is running. The specific color pattern depends on which command you chose.

**Timeout or completion**: LED returns to normal green status.

**UART Debug Output** (if UART is connected to `GPIO 0`/`1`):

Throughout Command Mode operation, the converter outputs detailed status messages on the UART port. These messages provide textual confirmation of every action:

- `"Command keys hold detected, waiting for 3 second hold..."` - Both activation keys pressed
- `"Command mode active! Press:"` - Mode activated (followed by menu)
- `"Log level selection: Press 1=ERROR, 2=INFO, 3=DEBUG"` - After pressing 'D'
- `"LED brightness selection: Press +/- to adjust (0-10), current=%u"` - After pressing 'L' (shows current level)
- `"Log level changed to ERROR"` - After selecting log level 1
- `"Log level changed to INFO"` - After selecting log level 2
- `"Log level changed to DEBUG"` - After selecting log level 3
- `"LED brightness increased to %u"` - After each + adjustment
- `"LED brightness decreased to %u"` - After each - adjustment
- `"Factory reset requested - restoring default configuration"` - When 'F' pressed
- `"Bootloader command received"` - When 'B' pressed
- Various timeout and abort messages

If you have a UART adapter connected (see [Logging documentation](logging.md)), you can monitor these messages in your terminal whilst using Command Mode. This is particularly helpful when troubleshooting or when you want confirmation that commands are being processed correctly, especially on setups without LED indicators.

### Keyboards with Non-Standard Shift Keys

Some keyboards don't have separate left and right shift keys, or their protocol sends identical scancodes for both. In these cases, the default Left Shift + Right Shift activation won't work—the converter can't distinguish between the two shifts.

Some keyboards have only one physical shift key (like the [Apple M0110A](../../src/keyboards/apple/m0110a/keyboard.h), where both sides report the same scancode), making the default combination impossible. For these keyboards, you can override the activation keys at compile time.

For example, the Apple M0110A uses Shift + Alt instead:

The override must appear in the keyboard's [`keyboard.h`](../../src/keyboards/apple/m0110a/keyboard.h) before the standard [`hid_keycodes.h`](../../src/common/lib/hid_keycodes.h) include. This ensures your keyboard-specific definition takes precedence over the default.

There's one constraint: both activation keys must be HID modifiers (Shift, Control, Alt, or GUI/Windows keys). This limitation exists because modifiers are processed differently from regular keys in the keystroke pipeline, making their simultaneous detection more reliable.

---

## Available Commands

Once Command Mode is active (indicated by the alternating green/blue LED), you can press one of five command keys. Each triggers a different operation.

### 'B' - Enter Bootloader Mode

Pressing 'B' immediately resets the RP2040 into its bootloader, called BOOTSEL mode. In this mode, the RP2040 appears to your computer as a USB mass storage device—a virtual drive you can drag files onto.

This is exactly the same state you'd enter by holding the physical BOOTSEL button whilst plugging in the board, but you're triggering it through software instead. It's particularly useful when the RP2040 is mounted inside a case or in a hard-to-reach location.

**How to use it**:

1. Activate Command Mode (both shifts, 3 seconds, wait for alternating LED)
2. Press the **'B'** key
3. The converter immediately resets. Your computer will see the RP2040 disconnect and then reconnect as a mass storage device, usually named "RPI-RP2"
4. Open the drive in your file manager and drag a new `.uf2` firmware file onto it
5. The RP2040 automatically flashes the new firmware and reboots as a converter again

This makes firmware updates much handier. Instead of unplugging the converter, holding BOOTSEL, plugging it back in, releasing BOOTSEL, you just activate Command Mode and press B. Done.

The bootloader entry command doesn't ask for confirmation—pressing B immediately triggers the reset. This is intentional: you've already gone through the deliberate activation process (hold both shifts for 3 seconds), so a second confirmation would be redundant.

Before resetting, the converter flushes any pending UART log messages to ensure you don't lose debug output. Then it calls the RP2040's built-in bootloader reset function ([`reset_usb_boot`](../../src/common/lib/command_mode.c) from `pico/bootrom.h`), which is part of the Pico SDK's boot ROM API.

### 'D' - Adjust Log Level

Pressing 'D' opens a submenu for changing how much debug information the converter outputs on its UART port (`GPIO 0` and `1` by default). This setting controls the verbosity of the logging system.

**How to use it**:

1. Activate Command Mode (both shifts, 3 seconds)
2. Press the **'D'** key
3. The LED changes to alternating green/pink, indicating you're in the log level submenu
4. Press one of three number keys:
   - **'1'** = ERROR level (minimal output, only critical problems)
   - **'2'** = INFO level (moderate output, includes initialisation and state changes)
   - **'3'** = DEBUG level (verbose output, every scancode and timing detail)
5. The LED flashes briefly to confirm, and the new log level takes effect immediately

The log level setting is saved to flash memory, so it persists across reboots. You don't need to re-set it every time you power cycle the converter.

**What each level shows**:

**ERROR level** (1): Only outputs messages about things going wrong—protocol errors, hardware failures, unexpected states. Use this when the converter is working fine and you don't want extra noise on the UART.

**INFO level** (2): The default setting. Outputs initialisation messages, protocol state changes, configuration loads, and errors. Gives you a good overview of what the converter is doing without overwhelming you with detail.

**DEBUG level** (3): Outputs everything—every scancode received, timing measurements, state machine transitions, HID reports sent. Use this when troubleshooting protocol issues or understanding exactly what's happening with a specific key.

If you don't have a UART adapter connected, changing the log level doesn't affect anything visible—the logging system still runs, it just outputs to a port nothing is listening to. But setting it correctly before you connect UART saves you from having to adjust it later.

For more details on the logging system and how to connect a UART adapter, see the [Logging documentation](logging.md).

### 'F' - Factory Reset

Pressing 'F' wipes all saved configuration settings and restores factory defaults. This affects settings stored in flash memory—specifically log level and LED brightness.

**How to use it**:

1. Activate Command Mode (both shifts, 3 seconds)
2. Press the **'F'** key
3. The converter immediately begins the factory reset process
4. The LED changes to solid magenta to indicate the device is resetting
5. After about one second, the device reboots automatically
6. After reboot, configuration is restored to defaults: INFO log level and default LED brightness

Factory reset is useful when you've made configuration changes and want to start fresh, or if you're handing the converter to someone else and want to clear your custom settings.

The reset happens immediately without confirmation—again, the deliberate Command Mode activation process serves as your confirmation. Unlike the other commands, factory reset triggers a device reboot to ensure all systems reinitialise with the clean configuration.

Technically, factory reset works by erasing the entire configuration storage sector in flash memory, reloading default values, saving them to flash, then using the watchdog timer to trigger a clean reboot. The process is designed to be safe even if power is lost partway through, thanks to the dual-copy storage architecture. Details in the [Configuration Storage documentation](config-storage.md).

### 'L' - Adjust LED Brightness

Pressing 'L' opens a submenu for adjusting the brightness of the converter's WS2812 RGB status LED. This is particularly useful if you have bright RGB LEDs that are distracting in a dark room, or if you want them brighter in a well-lit environment.

**How to use it**:

1. Activate Command Mode (both shifts, 3 seconds)
2. Press the **'L'** key
3. The LED begins cycling through a rainbow pattern (red → yellow → green → cyan → blue → magenta → red...), indicating you're in brightness adjustment mode
4. Press **'+'** (or **'='**, which is the same physical key) to increase brightness by one level
5. Press **'-'** to decrease brightness by one level
6. The current brightness applies immediately as you adjust—you can see the effect in real-time with the rainbow cycling
7. Brightness ranges from 0 (off) to 10 (maximum)
8. After 3 seconds of no input, the converter automatically saves the new brightness to flash and exits to normal operation

Each press of + or - resets the 3-second timeout, so you can make multiple adjustments without the mode timing out. Once you stop pressing keys, the 3-second timer counts down and the new brightness is saved.

The brightness setting persists across reboots—you only need to set it once.

Brightness adjustment uses [gamma correction](../../src/common/lib/ws2812/ws2812.c) to make the perceived brightness change linearly as you step through the levels. Without gamma correction, LED brightness appears to jump from very dim to blindingly bright in just a couple steps. With correction, each level increment produces an approximately equal perceived brightness change. The gamma-corrected brightness lookup table compensates for the non-linear response of human vision, using a power curve (γ ≈ 2.5) to ensure each brightness step appears visually equal.

The rainbow cycling during adjustment serves two purposes: it provides engaging visual feedback that you're in adjustment mode, and it lets you see how the current brightness level affects LED visibility across different colors.

For more about WS2812 LED hardware and configuration, see the [LED Support documentation](led-support.md).

### 'S' - Toggle Shift-Override

Pressing 'S' toggles the shift-override feature on or off for keyboards with non-standard shift legends. This only applies to keyboards that define a `keymap_shift_override_layers` array; other keyboards ignore the toggle.

**How to use it**:

1. Activate Command Mode (both shifts, 3 seconds)
2. Press the **'S'** key
3. The converter immediately toggles the shift-override state
4. The LED flashes briefly to confirm, and Command Mode exits
5. UART shows: `"Shift-Override enabled"` or `"Shift-Override disabled"`

**What shift-override does**:

Some keyboards have non-standard shift legends printed on the keycaps—terminal keyboards, vintage keyboards, or international layouts where the physical labels don't match modern HID standards. For example, the IBM Model M Type 2 (M122) has `"` (double-quote) printed above the `2` key, not `@`.

When shift-override is **enabled**, the converter remaps shifted keys to match these physical legends. When **disabled** (default), all keyboards behave with standard modern shift mappings regardless of what's printed on the keycaps.

**Keyboard compatibility**:

Shift-Override only works if the keyboard's firmware defines shifted character mappings. Check your keyboard's documentation in `src/keyboards/` to see if it supports shift-override. If the keyboard doesn't define custom shift mappings, pressing 'S' displays a warning message and the setting is not changed.

The shift-override state persists across reboots and is stored in flash memory. However:
- If you flash firmware for a different keyboard (different make/model/protocol), the state automatically resets to the disabled state.
- If you modify the keyboard firmware to remove the shift-override array but keep the same keyboard ID, the converter detects this on boot and automatically disables shift-override.

These validation checks ensure the config state always matches the actual keyboard capabilities.

For details on implementing shift-override for a new keyboard, see the [Adding Keyboards guide](../development/adding-keyboards.md#shift-override-non-standard-shift-legends).

---

## How Command Mode Works Internally

Understanding the implementation helps explain why Command Mode behaves the way it does and what limitations exist.

### Non-Blocking State Machine

Command Mode is implemented as a non-blocking state machine that runs in the main loop alongside keystroke processing. This means it never prevents the converter from receiving keyboard input or sending USB reports.

The state machine tracks several things:
- Which modifier keys are currently held
- How long they've been held
- Whether Command Mode is active
- Which command or submenu is awaiting input
- When to timeout and return to normal operation

Every time through the main loop, the state machine checks if conditions have changed. If both activation keys have been held for 3 seconds, it transitions to Command Mode active. If a timeout occurs, it transitions back to idle. Everything happens through time-based state checks rather than blocking delays.

### Time Tracking Without Blocking

The converter can't use functions like `sleep_ms()` or `busy_wait_us()` because those would block the main loop and cause the converter to miss keyboard scancodes or USB polls. Instead, Command Mode tracks time using `get_absolute_time()` and compares elapsed time on each main loop iteration.

For example, to implement the 3-second hold requirement:
1. When both activation keys are first pressed simultaneously, record the current time
2. On each main loop iteration, check if the keys are still held
3. Calculate elapsed time: current time minus recorded time
4. If elapsed time reaches 3000 milliseconds and keys are still held, activate Command Mode

This approach allows the converter to continue processing keystrokes and USB communication whilst tracking modifier hold duration in the background.

### HID Report Suppression

Whilst Command Mode is active, the converter suppresses normal HID reports to prevent command keys from appearing as regular keystrokes. When you press 'B' to enter bootloader mode, you don't want your computer to also type a 'b' character.

The suppression logic checks the Command Mode state before generating HID reports. If Command Mode is active, regular keystrokes are dropped—though the converter still receives them and processes their scancodes, it just doesn't forward them to the USB host.

Modifier keys used for activation (like the shifts) are treated specially. Whilst you're holding both shifts to activate Command Mode, the converter will still report them as pressed to the host for the duration of `SHIFT_HOLD_WAIT`, it will then send an empty keyboard report to the host on transition to Command Mode active. This prevents applications from interpreting the prolonged shift hold as a shift key stuck down.

### LED Feedback

Throughout Command Mode operations, the LED provides visual feedback about what's happening. These patterns are generated by the LED subsystem based on the current Command Mode state.

The LED update functions run in the main loop and check Command Mode state to decide which pattern to show. For example:
- If Command Mode is active but no command selected: alternate green/blue
- If 'D' command active awaiting log level: alternate green/pink
- If 'L' command active awaiting brightness: rainbow cycling through all colors

The LED patterns use non-blocking timing as well—tracking when to toggle colors based on elapsed time rather than using delays.

---

## Keyboard-Specific Configuration

Some keyboards require different activation key combinations due to hardware limitations or layout constraints. The converter allows keyboards to override the default Left Shift + Right Shift activation in their configuration headers.

### How Overrides Work

The override system uses conditional compilation with C preprocessor guards. The main Command Mode code defines default activation keys:

```c
#ifndef CMD_MODE_KEY1
#define CMD_MODE_KEY1 KC_LSHIFT
#endif

#ifndef CMD_MODE_KEY2
#define CMD_MODE_KEY2 KC_RSHIFT
#endif
```

The `#ifndef` directive means "only define this if it hasn't been defined already." This implements a "first definition wins" strategy.

Keyboard-specific headers (like `keyboards/apple/m0110a/keyboard.h`) can define these symbols before including the HID keycodes header:

```c
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_LALT
#include "hid_keycodes.h"
```

When the main Command Mode code later tries to define the defaults, the `#ifndef` guard sees they're already defined and skips the defaults. The keyboard-specific values take effect.

### Why Modifiers Only?

The activation keys must be HID modifiers (Shift, Control, Alt, GUI) because of how the keystroke pipeline processes them differently from regular keys.

Modifiers are handled in the first byte of the HID report and can all be detected simultaneously. Regular keys go into the six key slots and require more complex logic to detect specific combinations. Using modifiers for activation simplifies the detection code and makes it more reliable.

Additionally, modifier combinations are less likely to conflict with application shortcuts. Holding two shifts rarely triggers any application behavior, whilst holding two regular keys might activate application-specific features.

---

## Error Handling

Command Mode includes several safety mechanisms to prevent accidental activation and handle edge cases gracefully.

### Timeout Protection

If you activate Command Mode but don't press a command key within 3 seconds, it automatically exits back to normal operation. This prevents the converter from getting stuck in Command Mode if you activate it accidentally or change your mind.

The same timeout applies to command submenus. After pressing 'D' or 'L', if you don't press a selection key within 3 seconds, Command Mode exits. You won't end up with the converter permanently stuck waiting for a brightness setting.

### Invalid Key Handling

If you press a key that isn't a valid command (for example, pressing 'X' instead of 'B', 'D', 'F', or 'L'), Command Mode simply ignores it. The timeout continues running, so if you press the wrong key, you can just press the correct one within the 3-second window.

In submenus, the same logic applies. Pressing a key that isn't a valid option (pressing '4' for log level when only '1', '2', '3' are valid) gets ignored.

### Storage Failure

When commands try to save settings to flash (log level adjustment, LED brightness, factory reset), they check the return status from the configuration storage system. If flash write fails (extremely rare but possible if flash is worn out), the command still completes but the setting won't persist across reboots.

The LED feedback indicates success regardless of whether flash write worked. This is intentional—flash write failures are so rare that adding separate error feedback would complicate the interface for an edge case that users will likely never encounter.

---

## Usage Examples

Here are complete walkthroughs of common Command Mode operations.

### Updating Firmware

Suppose you've downloaded a new firmware version and want to install it:

1. Make sure your keyboard is plugged in and working
2. Press and hold both shift keys
3. Watch the status LED—after 3 seconds it starts alternating green/blue
4. Release the shift keys
5. Press the 'B' key
6. The converter resets, and your computer shows a new drive called "RPI-RP2"
7. Open the drive and drag your `.uf2` firmware file onto it
8. The drive automatically ejects and the converter reboots with the new firmware
9. Start typing—no configuration needed

### Enabling Verbose Debug Output

You're troubleshooting a key that doesn't work correctly and want to see exactly what scancodes your keyboard sends:

1. Connect a USB-UART adapter to `GPIO 0` (TX) and `GPIO 1` (RX)
2. Open a terminal program (like `screen /dev/ttyUSB0 115200`)
3. On your keyboard, press and hold both shifts for 3 seconds (LED alternates green/blue)
4. Release shifts and press 'D'
5. LED changes to alternating green/pink
6. Press '3' for DEBUG level
7. LED flashes briefly and returns to solid green
8. Now press keys and watch the terminal—you'll see detailed scancode information

### Dimming Bright LEDs

You've added WS2812 RGB LEDs to your converter but they're too bright at night:

1. Press and hold both shifts for 3 seconds (LED alternates green/blue)
2. Release shifts and press 'L'
3. LED begins cycling through rainbow colors
4. Press '-' several times to decrease brightness to a comfortable level
5. Wait 3 seconds—the converter saves the new brightness and exits to normal operation
6. The brightness setting persists after power cycling

---

## Troubleshooting

### Command Mode Won't Activate

If holding both shifts doesn't activate Command Mode after 3 seconds:

**Check if your keyboard supports it**: Keyboards with only one shift key or protocols that don't distinguish left/right shift need a configuration override. Check if your keyboard model has an override defined.

**Verify both shifts are detected**: Some keyboard layouts or scan code sets map both shifts to the same HID keycode. Connect UART and watch the debug output—you should see separate Left Shift and Right Shift make/break events when you press each one.

**Try a different key combination**: If your keyboard config defines a custom activation (like Shift + Alt), use that instead of the default.

### LED Doesn't Change

If the LED doesn't show the alternating green/blue pattern:

**Check WS2812 LED connection**: Verify your WS2812 LED is properly wired (data line, power, and ground). The converter uses WS2812 RGB LEDs for status indication.

**Verify LED is functional**: Check that the status LED works during normal operation (solid green when ready). If it doesn't work normally, it won't work for Command Mode either.

**Check firmware configuration**: Ensure CONVERTER_LEDS is defined in your build configuration. If LED support wasn't compiled in, visual feedback won't work.

### Wrong Keys Activate Command Mode

If unintended key combinations trigger Command Mode:

**Check for stuck modifier keys**: If one activation key is stuck (physically or electronically), pressing the other one alone would trigger activation. Test each modifier key individually to ensure they release correctly.

**Verify keymap**: Incorrect keymap configuration might map unexpected keys as modifiers. Check your keyboard's layout configuration.

---

## Related Documentation

**Features using Command Mode**:
- [Configuration Storage](config-storage.md) - How settings persist in flash memory
- [Logging](logging.md) - UART debug output controlled by log level setting
- [LED Support](led-support.md) - Status indicators and brightness control

**Implementation details**:
- [`command_mode.c`](../../src/common/lib/command_mode.c) - State machine implementation
- [`command_mode.h`](../../src/common/lib/command_mode.h) - API and configuration

**Keyboard customisation**:
- [Building Firmware](../getting-started/building-firmware.md) - How to compile with custom configurations
- Protocol-specific documentation for your keyboard's activation key requirements

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
