# Flashing Firmware

**Time Required**: 5-10 minutes  
**Difficulty**: Beginner-friendly

Now that you've built your firmware, it's time to get it onto your RP2040 board. The good news is that this is one of the easiest parts of the whole process—you just put the board into bootloader mode and drag-and-drop a file. That's it. Once it's done, you'll have a working keyboard/mouse converter!

**What you'll accomplish:**
- Put your RP2040 into bootloader mode
- Install firmware using drag-and-drop
- Verify your converter is working
- Learn how to update firmware without physical button access (Command Mode)

**Why this is so simple:** The RP2040 has a built-in bootloader that appears as a USB drive when you activate it. No special software or complicated tools needed—you literally just copy a file to it like you would to a USB stick.

---

## Prerequisites

Before you can flash the firmware, you'll need:

- **Compiled firmware** - The `rp2040-converter.uf2` file from [Building Firmware](building-firmware.md)
- **Assembled hardware** - Your wired converter from [Hardware Setup](hardware-setup.md)
- **USB cable** - Connected from your RP2040 to your computer
- **Access to BOOT and RESET buttons** - On your RP2040 board (for first-time flash only)

**Note**: After this initial flash, you can update firmware using Command Mode—no need to access the buttons again!

---

## Understanding the Flashing Process

The RP2040 operates in two modes:

1. **Bootloader Mode** - Acts as a USB drive named `RPI-RP2`, ready to receive firmware
2. **Normal Mode** - Runs your converter firmware, translating keyboard/mouse input

**The flashing process is pretty simple:**
- You enter bootloader mode (the board appears as a USB drive)
- Copy the `.uf2` file to the drive
- The RP2040 automatically writes the firmware to its flash memory
- It reboots into normal mode
- Your converter starts working immediately

This is the same process for all RP2040 boards—it's a standard feature built into the chip.

---

## Step 1: Locate the BOOT and RESET Buttons

First, locate the buttons on your RP2040 board. Where they are depends on which board you're using:

### Raspberry Pi Pico

- **BOOTSEL button**: Small button labeled "BOOTSEL" near the USB connector
- **RESET**: Not present (use USB unplug/replug instead, or add external reset button)

### Waveshare RP2040-Zero

- **BOOT button**: Small button labeled "BOOT" or "B" 
- **RESET button**: Small button labeled "RESET" or "R"
- Located on the back of the board

(This is the board I use, by the way—it's compact and has both buttons, which makes life easier)

### Other RP2040 Boards

If you're using a different board, have a look at your board's documentation:
- BOOT/BOOTSEL button (always present—it's required for the bootloader)
- RESET button (may be absent on some boards)
- Some boards have a single button that serves both functions

**Tip**: If your board doesn't have a RESET button, you can enter bootloader mode by:
1. Disconnect USB cable
2. Hold BOOT button
3. Reconnect USB cable while holding BOOT
4. Release BOOT button

---

## Step 2: Enter Bootloader Mode

This is the important bit—you need to put your RP2040 into a state where it can receive new firmware.

### Method 1: Using BOOT + RESET Buttons (Recommended)

**If your board has both buttons:**

1. **Hold the BOOT button** - Press and hold (don't release yet)
2. **Press and release RESET** - Quickly tap the RESET button while holding BOOT
3. **Release BOOT button** - Let go of BOOT
4. **Verify**: A USB drive named `RPI-RP2` should appear on your computer

**Visual sequence:**
```
BOOT:   Press ████████████████████░░░░ Release
RESET:        ░░░░ Tap ░░░░░░░░░░░░░░░
Time:   0s    1s         2s         3s
```

### Method 2: Using BOOT Button Only (No RESET Button)

**If your board lacks a RESET button:**

1. **Disconnect USB cable** - Unplug from RP2040
2. **Hold BOOT button** - Press and hold
3. **Reconnect USB cable** - Plug back in while holding BOOT
4. **Release BOOT button** - Let go after 2 seconds
5. **Verify**: `RPI-RP2` drive appears

### What You Should See

**On Windows:**
- File Explorer shows a new drive: `RPI-RP2 (D:)` or similar
- The drive contains two files: `INDEX.HTM` and `INFO_UF2.TXT`

**On macOS:**
- A new volume appears on your desktop: `RPI-RP2`
- It'll also show up in the Finder sidebar
- Contains the same two files

**On Linux:**
- The drive auto-mounts to `/media/<username>/RPI-RP2` or similar
- Shows the same two files

**Status LED (if installed):**
- **Will NOT turn magenta/purple** when using BOOT/RESET buttons
- LED will either remain **off** (if board was powered off)
- Or retain its **last state** (if board was already running when you pressed BOOT/RESET)
- Magenta LED is **only for Command Mode bootloader entry** (pressing 'B')

**Tip**: The `INFO_UF2.TXT` file contains your board's unique ID and bootloader version. This confirms you're in bootloader mode.

---

## Step 3: Flash the Firmware

Now you just need to copy your compiled firmware to the RP2040.

### Locate Your Firmware File

Your firmware file is sitting in your project's `build/` directory:

**Path**: `rp2040-keyboard-converter/build/rp2040-converter.uf2`

**File details:**
- Name: `rp2040-converter.uf2`
- Size: ~80-120 KB (depending on your configuration)
- This was created when you ran the build command

### Copy the Firmware

**Method 1: Drag and Drop (All Operating Systems)**

1. Open your file manager (Finder on macOS, Explorer on Windows, or your file browser on Linux)
2. Navigate to the `build/` folder in your project
3. Find `rp2040-converter.uf2`
4. **Drag the file** to the `RPI-RP2` drive
5. **Wait** - The file copies, then the drive disappears (the RP2040 reboots automatically)

**Method 2: Command Line (macOS/Linux)**

```bash
# From your project directory
cp build/rp2040-converter.uf2 /Volumes/RPI-RP2/
```

**Method 3: Command Line (Windows PowerShell)**

```powershell
# From your project directory
# Replace D: with your RPI-RP2 drive letter
Copy-Item build\rp2040-converter.uf2 D:\
```

### What Happens Next

**During flashing (takes 1-2 seconds):**
- The RP2040's onboard LED may blink rapidly (if there is one)
- The `RPI-RP2` drive disappears from your computer
- You might see a brief "drive removed" notification, or some other warning about being removed unexpectedly (that's normal)
- Your status LED (if you've installed one) remains in its previous state (it doesn't change during the BOOT/RESET method)

**After flashing:**
- The RP2040 automatically reboots
- Your new firmware starts running immediately
- The status LED shows orange (if an LED is installed—means it's waiting for device initialization), and should change to green if everything's OK!

**Important**: The `RPI-RP2` drive disappearing is **normal and expected**. This means the firmware was successfully written and the RP2040 rebooted into normal mode.

---

## Step 4: Verify It's Working

Now let's test your newly flashed converter to confirm everything's working as it should.

### Initial Power-Up Checks

**1. Status LED (if installed):**
- **🟠 Orange**: Waiting for keyboard initialization (normal at startup)
- **🟢 Green**: Converter ready (keyboard initialized successfully)
- **No light**: Either no LED installed, configuration or wiring issue

**2. Computer Recognition:**
- Your computer should detect a new USB keyboard (and mouse, if you built with mouse support)
- Windows: "Device connected" sound, Device Manager shows "USB Keyboard"
- macOS: Keyboard Setup Assistant may appear (click through to finish setup)
- Linux: Device appears in `lsusb` output
- **If using 4 LEDs**: You may see LEDs 2-4 (Num/Caps/Scroll Lock) change state as your computer sets the initial lock key status

### Testing Keyboard Input

**Basic functionality test:**

1. **Open a text editor** - Notepad (Windows), TextEdit (macOS), or whatever editor you prefer
2. **Type on your keyboard** - Try various keys
3. **Verify output appears** - The characters should appear in the editor
4. **Test special keys**:
   - Shift + letters (for uppercase)
   - Caps Lock (to ensure it toggles correctly)
   - Function keys (F1-F12)
   - Arrow keys, Home, End, Page Up/Down
   - Number pad (if your keyboard has one)

**Lock key indicators (if using 4 LEDs):**
- Press **Num Lock** - LED 2 should light up (unless you're connecting to a Apple Mac which doesnt use Num Lock)
- Press **Caps Lock** - LED 3 should light up
- Press **Scroll Lock** - LED 4 should light up

### Testing Mouse Input (if built with mouse support)

**Basic functionality test:**

1. **Move the mouse** - Cursor should move on screen
2. **Click buttons** - Left/right clicks should register
3. **Scroll wheel** (if mouse has one) - Page should scroll

---

## Step 5: Understanding Command Mode

Command Mode is one of those features that you'll really appreciate once your keyboard's all set up and installed somewhere awkward. It lets you update firmware and adjust settings **without having physical access to the RP2040 board**. Once your keyboard's tucked away inside a case or under a desk, Command Mode means you don't have to take everything apart just to update the firmware.

### What is Command Mode?

It's a special menu you can access by holding both shift keys for 3 seconds. Once you're in, you can:
- Enter bootloader mode for firmware updates (no BOOT button needed!)
- Adjust LED brightness
- Change debug log verbosity
- Reset to factory settings

**Why this is so useful**: After your initial flash, you'll never need to touch the BOOT and RESET buttons again. All firmware updates can be done through Command Mode.

### Entering Command Mode

**How to activate:**

1. **Press and hold BOTH shift keys** - Left Shift + Right Shift (don't press any other keys!)
2. **Hold for 3 seconds** - Status LED will start flashing green/blue
3. **Release shift keys** - You're now in Command Mode
4. **Press a command key** - See options below

**Important**: You must hold **ONLY** the two shift keys. If you press any other key (including modifiers like Ctrl, Alt, Fn), Command Mode won't activate.

**Note**: The default key combination is Left Shift + Right Shift, but this is configurable. Some keyboards (like those with a single shift key) may define different keys in their `keyboard.h` file. Check your specific keyboard's documentation if the default combination doesn't work.

**Visual feedback:**
- **Flashing Green/Blue (100ms intervals)**: Command Mode active, waiting for command
- **Times out after 3 seconds** if no key pressed

### Available Commands

Once Command Mode is active (flashing green/blue LED), press one of these keys:

| Key | Command | Description |
|-----|---------|-------------|
| **B** | **Bootloader** | Enter bootloader mode for firmware updates |
| **L** | **LED Brightness** | Adjust status LED brightness (0-10) |
| **D** | **Debug Level** | Change UART log verbosity (ERROR/INFO/DEBUG) |
| **F** | **Factory Reset** | Restore all settings to defaults and reboot |
| **S** | **Shift-Override** | Toggle shift-override for non-standard shift legends |

We'll cover the Bootloader command in detail next. The other commands are explained in the [Features documentation](../features/command-mode.md).

---

## Step 6: Updating Firmware (Command Mode Method)

After your initial flash, use Command Mode to update firmware without touching the BOOT button. This is especially handy when your converter is installed and you want to try a different keyboard configuration or install updated firmware.

### When to Use This Method

**Use Command Mode for updates when:**
- ✅ Your converter's already installed and working
- ✅ You've built new firmware and want to flash it
- ✅ The BOOT/RESET buttons are hard to access
- ✅ You want a quick, handy update process

**Use the BOOT button method when:**
- ❌ You're flashing for the first time (no firmware installed yet)
- ❌ The firmware is corrupted or not booting
- ❌ Command Mode isn't responding

### Update Process Using Command Mode

**Full steps:**

1. **Build your new firmware** - Follow [Building Firmware](building-firmware.md) to create updated `rp2040-converter.uf2`

2. **Enter Command Mode**:
   - Press and hold both shift keys for 3 seconds
   - LED flashes green/blue rapidly
   - Release shift keys

3. **Press 'B' key** - Enters bootloader mode
   - LED turns **magenta** (purple/pink)
   - `RPI-RP2` drive appears on your computer
   - **Important**: The RP2040 stays in bootloader mode until you either copy a firmware file OR disconnect/reconnect power. Your existing firmware and settings remain safe—nothing is erased unless you flash a new .uf2 file (even then, your saved settings are preserved). You can safely exit bootloader mode by disconnecting and reconnecting USB.

4. **Copy firmware file**:
   - Drag `rp2040-converter.uf2` to `RPI-RP2` drive
   - Or use command line: `cp build/rp2040-converter.uf2 /Volumes/RPI-RP2/`

5. **Automatic reboot**:
   - Drive disappears (normal!)
   - RP2040 reboots with new firmware
   - LED shows orange → green as converter initializes

**Tip**: Once you press 'B', the bootloader stays active indefinitely—take your time copying the firmware file. There's no timeout once you're in bootloader mode.

### Troubleshooting Command Mode Updates

**Problem: LED doesn't flash green/blue when holding shifts**
- Make sure you're holding **ONLY** the two shift keys (no other keys!)
- Hold for full 3 seconds
- Check that firmware supports Command Mode (all recent builds do)
- If your keyboard uses custom Command Mode keys (not the default Left+Right Shift), check your keyboard's documentation for the correct combination

**Problem: Pressed 'B' but no RPI-RP2 drive appears**
- LED should turn magenta—if not, 'B' key didn't register
- Try entering Command Mode again
- As fallback, use BOOT button method (Step 2)

**Problem: Pressed 'B', saw magenta LED, but drive didn't appear on computer**
- Wait a few seconds—some systems take longer to mount USB drives
- Try unplugging and reconnecting USB cable (will exit bootloader, then re-enter via Command Mode)
- Check USB cable and port are working
- As fallback, use BOOT button method (Step 2)

---

## Troubleshooting

Running into issues? Here are troubleshooting steps and solutions:

---

### ❌ RPI-RP2 drive doesn't appear

**What this means**: Your RP2040 didn't enter bootloader mode.

**How to fix it**:

1. **Try again with precise timing**:
   - Hold the BOOT button firmly (don't just tap it)
   - Press RESET whilst continuing to hold BOOT
   - Hold BOOT for 2 full seconds after releasing RESET

2. **Check your USB cable**:
   - Make sure you're using a data cable (not a charge-only cable)
   - Try a different USB port on your computer
   - Try a different USB cable

3. **Verify board functionality**:
   - Does the RP2040's onboard LED light up when USB is connected?
   - If not, the board may have a power issue

4. **Try the alternative method**:
   - Disconnect USB
   - Hold BOOT
   - Reconnect USB whilst holding BOOT
   - Wait 2 seconds, then release BOOT

---

### ❌ Drive appears, but file won't copy

**What this means**: File copy error or drive not writable.

**How to fix it**:

1. **Check file size**:
   - `rp2040-converter.uf2` should be 80-120 KB
   - If file is 0 KB or very large, rebuild firmware

2. **Check file name**:
   - Must end in `.uf2` extension
   - Case-sensitive on macOS/Linux

3. **Try command line copy** instead of drag-and-drop:
   ```bash
   # macOS/Linux
   cp build/rp2040-converter.uf2 /Volumes/RPI-RP2/
   
   # Windows
   copy build\rp2040-converter.uf2 D:\
   ```

4. **Check drive permissions**:
   - Drive should be writable (not read-only)
   - On Linux, you may need sudo: `sudo cp ...`

---

### ❌ Firmware copied, but converter doesn't work

**What this means**: The firmware flashed successfully, but something's not quite right with the configuration.

**How to fix it**:

1. **Check your Status LED** (if you've installed one):
   - **Orange**: Waiting for keyboard (check your keyboard power and wiring)
   - **Off**: Either no LED installed, or there's an LED wiring problem
   - **Rapid flashing**: Error state (check UART output for details)

2. **Verify your keyboard configuration**:
   - Did you build firmware for the correct keyboard model?
   - Example: An AT/PS2 keyboard needs AT/PS2 protocol firmware
   - Rebuild with the correct `KEYBOARD` parameter if needed

3. **Check your hardware connections**:
   - Verify the CLOCK and DATA connections from [Hardware Setup](hardware-setup.md)
   - Make sure the level shifter is powered (3.3V and 5V)
   - Check keyboard power (5V to device)

4. **Try a different keyboard** (if available):
   - This confirms if the issue is firmware or hardware
   - Use the same protocol (e.g., another AT/PS2 keyboard)

---

### ❌ Computer doesn't recognise converter as keyboard

**What this means**: RP2040 is powered, but USB HID interface not working.

**How to fix it**:

1. **Check Device Manager / System Info**:
   - **Windows**: Device Manager → Human Interface Devices
   - **macOS**: System Information → USB
   - **Linux**: Run `lsusb` in terminal
   - Look for "USB Keyboard" or "HID Device"

2. **Try different USB port**:
   - Use direct motherboard USB port (not hub)
   - USB 2.0 ports often more reliable than USB 3.0 for this

3. **Verify firmware built correctly**:
   - Check build output had no errors
   - Firmware file size reasonable (80-120 KB)

4. **Re-flash firmware**:
   - Enter bootloader mode again
   - Copy firmware file again
   - Ensure drive disappears (indicates successful flash)

---

### ❌ Some keys don't work or produce wrong characters

**What this means**: There's a keymap mismatch or scancode issue.

**How to fix it**:

1. **Verify your keyboard configuration**:
   - Are you using the right keyboard model?
   - Example: A Model M Enhanced vs Model M Classic have different layouts
   - Check [Supported Keyboards](../keyboards/README.md) for your exact model

2. **Test for a systematic pattern**:
   - Do specific keys consistently fail (e.g., all Function keys)?
   - Do keys produce the wrong but consistent output (e.g., 'A' types 'B')?
   - This helps identify whether it's a scancode or keymap issue

3. **Check for a protocol mismatch**:
   - An AT/PS2 keyboard needs AT/PS2 protocol firmware
   - An XT keyboard needs XT protocol firmware
   - A protocol mismatch causes unpredictable key behaviour

4. **Have a look at your keyboard documentation**:
   - Some keyboards have quirks or variations
   - See `src/keyboards/<your-keyboard>/README.md` for details

---

### 💡 Still having issues?

If none of these solutions work:

1. **Enable debug output**:
   - Connect UART adapter to RP2040 GPIO 0 (TX) and GPIO 1 (RX)
   - Use Command Mode ('D' key) to set debug level to 3 (DEBUG)
   - Watch serial output for error messages

2. **Try a known-good configuration**:
   - Build firmware with `modelm/enhanced` (standard AT/PS2 configuration)
   - Test with a standard PS/2 keyboard if available
   - Rules out configuration-specific issues

3. **Ask for help**: Visit [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) with:
   - Your keyboard model and protocol
   - Build command you used
   - Any error messages or unexpected behaviour
   - UART debug output (if available)

---

## What's Next?

Brilliant! You've now got yourself a fully working keyboard converter. 🎉

### Using Your Converter

Your converter's ready for daily use:
- Plug in your keyboard
- Connect the RP2040 to your computer via USB
- That's it—start typing! Your keyboard now works with modern computers

### Exploring Features

Learn more about what your converter can do:
- **[Command Mode](../features/command-mode.md)** - Full guide to all Command Mode features
- **[Keyboard Layouts](../keyboards/README.md)** - Details about specific keyboard models

### Customization and Advanced Use

Ready to go deeper?
- **[Custom Keymaps](../development/custom-keymaps.md)** - Remap keys to your preference
- **[Adding New Keyboards](../development/adding-keyboards.md)** - Support your own keyboard model
- **[Build System Details](../advanced/README.md)** - Understanding the build process

### Getting Help

If you run into issues or have questions:
- 💬 [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) - Community support
- 🐛 [Report Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues) - Bug reports and feature requests

---

## Quick Reference

Once you're familiar with the process, here's the condensed version:

**Initial flash (physical buttons):**
```
1. Hold BOOT button
2. Press and release RESET
3. Release BOOT
4. Copy rp2040-converter.uf2 to RPI-RP2 drive
5. Wait for automatic reboot
```

**Update firmware (Command Mode):**
```
1. Hold both shift keys for 3 seconds
2. Press 'B' key
3. Copy rp2040-converter.uf2 to RPI-RP2 drive
4. Wait for automatic reboot
```

**Command Mode quick keys:**
- **B** = Bootloader mode (for firmware updates)
- **L** = LED brightness adjustment
- **D** = Debug log level selection
- **F** = Factory reset
- **S** = Shift-override toggle

---

**[← Previous: Building Firmware](building-firmware.md)**
