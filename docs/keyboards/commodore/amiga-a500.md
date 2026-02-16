# Commodore Amiga Keyboard Family

**Configuration**: `amiga/a500`

---

## Overview

The Amiga keyboard from Commodore's Amiga computers uses a bidirectional serial protocol with a two-way handshake mechanism. The protocol itself is consistent across models, but the keyboards vary between different Amiga models—different connectors, slightly different scancodes, though they all use the same core protocol.

This configuration targets the A500/A500+ layout, which is what I've had access to for testing. The A2000 and A3000 keyboards use the same basic protocol and similar layouts, but they haven't been tested yet with this firmware.

**Compatible Systems**: Amiga 500, Amiga 500+, Amiga 2000 (untested), Amiga 3000 (untested)

---

## Specifications

| Specification | Details |
|---------------|---------|
| **Make** | Commodore |
| **Model** | Amiga Keyboard (A500/A500+/A2000/A3000) |
| **Keys** | 94-101 keys depending on region |
| **Protocol** | Amiga bidirectional serial with handshake |
| **Codeset** | Amiga scancode set |
| **Connector** | Varies by model—see Hardware Connection below |
| **Voltage** | 5V |
| **Switch Type** | Mitsumi or Samsung (A500/A500+) |
| **Layout** | Amiga-specific with Amiga keys, BAE or ISO variants |

---

## Building Firmware

Build firmware for Amiga keyboards:

```bash
# Amiga keyboard only
docker compose run --rm -e KEYBOARD="amiga/a500" builder

# Amiga keyboard + AT/PS2 Mouse
docker compose run --rm -e KEYBOARD="amiga/a500" -e MOUSE="at-ps2" builder
```

**Output**: `build/rp2040-converter.uf2`

**Note**: The `MOUSE="at-ps2"` option adds support for standard AT/PS2 mice, not Amiga mice. Amiga mouse protocol isn't currently supported.

See: [Building Firmware Guide](../../getting-started/building-firmware.md)

---

## Key Mapping

The default keymap preserves the Amiga keyboard layout whilst mapping Amiga-specific keys to modern equivalents.

### Layout Overview

**Base Layer - Physical Layout** (from [`keyboard.h`](../../../src/keyboards/amiga/a500/keyboard.h)):
```
Commodore Amiga 500/2000 Keyboard Layout

,---. ,------------------------. ,------------------------.
|Esc| |F1  |F2  |F3  |F4  |F5  | |F6  |F7  |F8  |F9  |F10 |
`---' `------------------------' `------------------------'
,-------------------------------------------------------------. ,-----------. ,---------------.
|  `  |  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|  \|Bsp| |Del  |Help | |  (|  )|  /|  *|
|-------------------------------------------------------------| `-----------' |---------------|
|  Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|     |               |  7|  8|  9|  -|
|--------------------------------------------------------\Entr|     ,---.     |---------------|
|Ctrl|Cap|  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|  #|    |     |Up |     |  4|  5|  6|  +|
|-------------------------------------------------------------| ,-----------. |---------------|
|Shift |  <|  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|  Shift   | |Lef|Dow|Rig| |  1|  2|  3|   |
`-------------------------------------------------------------' `-----------' |-----------|Ent|
    |Alt |A   |            Space                  |A   |Alt |                 |      0|  .|   |
    `-------------------------------------------------------'                 `---------------'
```

**Raw Scancode Map (in hex)** (from [`keyboard.h`](../../../src/keyboards/amiga/a500/keyboard.h)):
```
,---. ,------------------------. ,------------------------.
| 45| | 50 | 51 | 52 | 53 | 54 | | 55 | 56 | 57 | 58 | 59 |
`---' `------------------------' `------------------------'
,-------------------------------------------------------------. ,-----------. ,---------------.
| 00  | 01| 02| 03| 04| 05| 06| 07| 08| 09| 0A| 0B| 0C| 0D| 41| | 46  | 5F  | | 5A| 5B| 5C| 5D|
|-------------------------------------------------------------| `-----------' |---------------|
| 42    | 10| 11| 12| 13| 14| 15| 16| 17| 18| 19| 1A| 1B|     |               | 3D| 3E| 3F| 4A|
|--------------------------------------------------------\ 44 |     ,---.     |---------------|
| 63 | 62| 20| 21| 22| 23| 24| 25| 26| 27| 28| 29| 2A| 2B|    |     | 4C|     | 2D| 2E| 2F| 5E|
|-------------------------------------------------------------| ,-----------. |---------------|
| 60   | 30| 31| 32| 33| 34| 35| 36| 37| 38| 39| 3A|    61    | | 4F| 4D| 4E| | 1D| 1E| 1F|   |
`-------------------------------------------------------------' `-----------' |-----------| 43|
    | 64 | 66 |              40                   | 67 | 65 |                 |     0F| 3C|   |
    `-------------------------------------------------------'                 `---------------'
```

**Special Keys** (from [`keyboard.h`](../../../src/keyboards/amiga/a500/keyboard.h)):
- **Left Amiga**: `0x66` - Mapped to Left GUI (Windows/Command key)
- **Right Amiga**: `0x67` - Mapped to Right GUI (Windows/Command key)
- **Help**: `0x5F` - Acts like a function key, can be mapped to Insert/Help
- **Keypad ()**: `0x5A`, `0x5B` - Unique to Amiga keyboards
- **UK # key**: `0x2B` - Used on UK/ISO layout (backslash on US layout)
- **European <> key**: `0x30` - Between Left Shift and Z on European keyboards

### Amiga-Specific Keys

| Amiga Key | Modern Mapping | Notes |
|-----------|----------------|-------|
| **Left Amiga (⌘)** | Left GUI | Windows Key / Command |
| **Right Amiga (⌘)** | Right GUI | Windows Key / Command |
| **Help** | Help / Insert | Can be mapped to Help or Insert |
| **Caps Lock** | Caps Lock | LED controlled by keyboard itself |
| **Keypad ()** | Keypad ( / ) | Unique to Amiga keyboards |

---

## Customization

### Modifying Key Layout

To customise the key layout, edit the keymap in [`keyboard.c`](../../../src/keyboards/amiga/a500/keyboard.c). Available keycodes are defined in [`hid_keycodes.h`](../../../src/common/lib/hid_keycodes.h).

### Command Mode Keys

This keyboard uses the default command mode keys: **Left Shift + Right Shift**

Hold for 3 seconds to enter command mode (bootloader entry).

---

## Hardware Connection

### Connector and Pinout

Amiga keyboards use different physical connectors depending on the model—A500/A500+ use an 8-pin internal header, A2000/A3000 use DIN-5 180°, and the A1000 uses RJ10 4P4C. All compatible models (A500/A500+/A2000/A3000) use the same electrical pinout, so you'll need to adapt the connector to match your specific Amiga model.

**Pinout details and diagrams**: See [Amiga Protocol - Physical Interface](../../protocols/amiga.md#physical-interface) for complete connector pinout diagrams and specifications.

### Wiring to RP2040

Connect the keyboard to your Raspberry Pi Pico (signal mapping is the same for all connector types):

| Amiga Signal | Function | RP2040 GPIO | Notes |
|--------------|----------|-------------|-------|
| **DATA** | Data | GPIO 2 (default) | Configurable in [`config.h`](../../../src/config.h) |
| **CLOCK** | Clock | GPIO 3 (DATA+1) | Must be DATA pin + 1 |
| **VCC** | +5V | VBUS 5V | External 5V recommended |
| **GND** | Ground | GND | Any ground pin |

**Important**: CLOCK pin must be DATA pin + 1—that's a hardware constraint. If you change DATA to GPIO 10, CLOCK becomes GPIO 11.

See: [Hardware Setup Guide](../../getting-started/hardware-setup.md)

---

## Protocol Details

The Amiga keyboard uses a bidirectional protocol with two-way handshake mechanism:

- **Device to Host**: Keyboard sends scancodes only
- **Host to Device**: Converter cannot send commands—Caps Lock LED's controlled by the keyboard itself
- **Handshake**: Two-way handshake with DATA line, keyboard controls communication
- **Scancode Set**: Amiga-specific scancode set, not XT/AT compatible
- **Clock Frequency**: Approximately 60-85 μs bit period, roughly 12-17 kHz

See: [Amiga Protocol Documentation](../../protocols/amiga.md)

---

## History & Variants

The Amiga keyboards were produced for the A500, A500+, A2000, and A3000 models. These keyboards came in both US BAE (Big Ass Enter) and European ISO layouts with regional variants for UK, German, French, and other markets.

| Amiga Model | Keyboard Type | Connector | Switch Type | Layout | Compatibility |
|-------------|---------------|-----------|-------------|--------|---------------|
| **A500** | Integrated | 8-pin internal | Mitsumi or Samsung | US BAE | ✅ Tested |
| **A500+** | Integrated | 8-pin internal | Mitsumi | ISO | ✅ Tested |
| **A1000** | External | RJ10 4P4C | Unknown | Different | ❌ Not Compatible |
| **A2000** | External | DIN-5 180° | Unknown | US BAE | ⚠️ Not Tested |
| **A3000** | External | DIN-5 180° | Unknown | US BAE | ⚠️ Not Tested |

---

## Troubleshooting

### Keyboard Not Detected

First thing to check is your wiring—make sure the DATA and CLOCK pins match what's in [`config.h`](../../../src/config.h), which by default is DATA on GPIO2 and CLOCK on GPIO3. Check you have stable 5V power too—use an external supply rather than relying on the Pico's VBUS directly if you can. Make sure you have the correct connector for your specific Amiga model, and check the cable for broken wires with a continuity tester.

Amiga keyboards need a power cycle to initialize properly, so if you've just plugged it in, give it a power cycle. If you're using an A500, A500+, A2000, or A3000 keyboard, it should work fine. The A1000 isn't compatible—it's got a completely different layout. If you're using one of the untested models like the A2000 or A3000 and running into issues, let me know so I can investigate.

### Keys Not Registering

Mitsumi and Samsung switches (used in A500/A500+) can accumulate dirt over time—disassemble and give them a clean if needed. Individual switches can also fail outright, so test them if you're having issues. Some Amiga keyboards use membrane layers, and these can fail over time. The keyboard controller chip might also be damaged if you're having widespread issues. If you're using an ISO layout keyboard, be aware some keys might be in different positions than you'd expect.

### Caps Lock LED Not Working

This is expected behaviour. The Amiga protocol doesn't support host-controlled LEDs—the Caps Lock LED's controlled by the keyboard itself, not the converter. The keyboard toggles its own LED based on Caps Lock key presses. The converter can't control the Caps Lock LED state—it's a protocol limitation, not a firmware issue.

### Handshake Timing Issues

The Amiga protocol's quite timing-sensitive—it needs precise 60-85 μs handshake timing. Long cables can cause timing issues, so keep them short if you're having problems. Electrical noise can also be a factor, so shielded cables might help in noisy environments. Unstable 5V power can cause communication errors too.

### Wrong Scancode Interpretation

Amiga scancodes are different from XT/AT scancodes, so verify the protocol's been detected correctly. Check the scancode mapping in [`keyboard.c`](../../../src/keyboards/amiga/a500/keyboard.c) if you think something's off. ISO layouts like the A500+ may have additional or relocated keys compared to the US BAE layout. If you're using one of the untested models and finding scancode mismatches, report them so I can update the mappings.

---

## Source Files

- **Configuration**: [`src/keyboards/amiga/a500/keyboard.config`](../../../src/keyboards/amiga/a500/keyboard.config)
- **Keymap**: [`src/keyboards/amiga/a500/keyboard.c`](../../../src/keyboards/amiga/a500/keyboard.c)
- **Header**: [`src/keyboards/amiga/a500/keyboard.h`](../../../src/keyboards/amiga/a500/keyboard.h)

---

## Related Documentation

- [Supported Keyboards](../README.md) - All supported keyboards
- [Amiga Protocol](../../protocols/amiga.md) - Protocol details
- [Hardware Setup](../../getting-started/hardware-setup.md) - Wiring guide
- [Building Firmware](../../getting-started/building-firmware.md) - Build instructions
- [Command Mode](../../features/README.md) - Command mode features

---

## External Resources

### Protocol Documentation

- **[Amiga Hardware Manual - Keyboard Interface](http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0114.html)** - Official Commodore Amiga keyboard hardware documentation
- **[Amiga Keyboard Protocol Specification](http://amiga.nvg.org/amiga/keyboard.html)** - Detailed protocol timing and implementation

### Community Resources

- [Deskthority Wiki: Amiga Keyboards](https://deskthority.net/wiki/Commodore_Amiga_keyboard) - Keyboard variants and technical information
- [English Amiga Board](https://eab.abime.net/) - Active Amiga community

