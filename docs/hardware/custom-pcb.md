# Custom PCB Design

When I found an IBM Model F PC/AT keyboard (model 6450225) in my parents' attic, I knew I wanted to convert it properly—not with an external adapter and messy cables, but with something that'd fit neatly inside the case. This custom PCB is the result of that project, and I'm sharing the design files and documentation here in case anyone else wants to build their own.

## Table of Contents

- [Why Build a Custom PCB?](#why-build-a-custom-pcb)
- [Design and Specifications](#design-and-specifications)
- [PCB Layout](#pcb-layout)
- [Manufacturing](#manufacturing)
- [Installation](#installation)
- [Design Iterations](#design-iterations)
- [Fabrication Files](#fabrication-files)
- [Related Documentation](#related-documentation)

---

## Why Build a Custom PCB?

The breadboard prototype worked perfectly well for testing, but it's not something you'd want to live with long-term. A custom PCB solves several problems:

**What it gives you:**
- Everything on one neat board—no loose wiring inside the keyboard
- Fits in the available space without modifying the case
- Only the USB cable exits the keyboard (looks stock from the outside)
- Proper level shifting built in (no separate modules)
- Reliable connections that won't work loose

**What it includes:**
- RP2040 microcontroller
- Bi-directional level shifters (5V ↔ 3.3V)
- USB Type-C connector
- All the supporting components (power regulation, decoupling, etc.)

---

## Design and Specifications

I designed this in EasyEDA, which is free and has decent integration with JLCPCB's manufacturing service. The board measures 40×30mm—specifically designed to fit inside the IBM Model F PC/AT (model 6450225) and mount using the existing screw holes on the internal frame.

**What's on the board:**
- **RP2040 microcontroller** - Same chip as the Raspberry Pi Pico
- **Level shifters** - BSS138 MOSFETs for bi-directional 5V ↔ 3.3V translation
- **USB Type-C connector** - Much nicer than Micro-B
- **Power regulation** - 3.3V for the RP2040 from USB 5V
- **Decoupling capacitors** - Standard RP2040 requirements
- **Crystal oscillator** - 12MHz for USB timing

**GPIO connections:**
- Keyboard CLOCK: GPIO 17
- Keyboard DATA: GPIO 16
- Status LED: GPIO 25 (built-in RP2040 LED)
- USB: Native RP2040 USB (GPIO 20/21)

The PCB is 1.6mm thick (standard), 1oz copper, with HASL lead-free finish. Nothing exotic—just bog-standard PCB specs that any manufacturer can handle.

---

## PCB Layout

Here's what the design looks like. I went through a couple of revisions before settling on this layout—the first attempt had terrible component placement and didn't fit the keyboard properly.

### PCB Trace Design (Top)
![PCB Trace Top](../images/hardware/PCB-Diag-Top.png)

### PCB Trace Design (Bottom)
![PCB Trace Bottom](../images/hardware/PCB-Diag-Bottom.png)

### 2D PCB Render (Top)
![2D PCB Top](../images/hardware/2D-PCB-Top.png)

### 2D PCB Render (Bottom)
![2D PCB Bottom](../images/hardware/2D-PCB-Bottom.png)

### 3D PCB Render (Top)
![3D PCB Top](../images/hardware/3D-PCB-Top.png)

### 3D PCB Render (Bottom)
![3D PCB Bottom](../images/hardware/3D-PCB-Bottom.png)

---

## Manufacturing

I used JLCPCB for fabrication and assembly. They're quite affordable compared to other manufacturers, and their SMT assembly service is really handy—you upload your Gerber files, BOM, and pick-and-place file, and they'll build the boards for you.

### Manufactured PCBs
![Manufactured PCBs from JLCPCB](../images/hardware/jlcpcb-boards.png)

**The process:**
1. Upload Gerber files to JLCPCB
2. Select PCB specs (1.6mm, 1oz copper, HASL lead-free)
3. Enable SMT assembly service
4. Upload BOM and pick-and-place files
5. Review component availability (some might be out of stock)
6. Confirm and order

**What you get:**
Minimum order is 5 PCBs. I paid about £5 per board when ordering 5 with assembly (I had some SMT assembly vouchers at the time, so your price will vary), plus shipping. Lead time was about 2-3 weeks to the UK. Even at full price, it works out cheaper than buying all the individual components and spending hours soldering them yourself.

---

## Installation

### Installed Converter Board
![Installed converter inside keyboard](../images/hardware/installed-board.png)

Installing the board is simple—it mounts on the keyboard's internal frame, so you're not drilling or modifying anything permanent.

**Steps:**
1. Open the keyboard case (see my [separate case video](https://www.paulbramhall.uk/bits-n-bobs/keyboards/1986-ibm-model-f-at/separating-the-case-of-the-ibm-model-f-at/) if you need help with this—it's a bit tricky)
2. Locate the keyboard controller board
3. Remove the existing cable from the controller
4. Connect the converter to the controller's output connector
5. Attach the Ground cable from the backplate to the converter board
6. Route the USB cable through the existing cable channel
7. Close the case

From the outside, it looks completely stock. The only giveaway is the USB-C cable instead of the original DIN-5 connector.

### Fully Assembled Board
![Fully assembled converter board](../images/hardware/assembled_6450225_1.png)

---

## Design Iterations

This is actually Revision 2 of the design. The first version was more of a test-fit to get measurements—the component placement was terrible, and it didn't fit the keyboard case properly. Live and learn.

**Changes in Rev 2:**
- Completely redid component placement
- Optimized trace routing (fewer vias, cleaner layout)
- Adjusted board outline to fit the case better
- Added proper level shifter circuit (Rev 1 didn't have this)

If you're using the fabrication files, you're getting Rev 2—that's the current version that actually works properly.

---

## Fabrication Files

All the files you need to manufacture this PCB are in the [`fabrication/`](fabrication/) directory:

**What's included:**
- **[Schematic (PDF)](../images/hardware/Schematic.pdf)** - Complete electrical schematic
- **Gerber files** - PCB manufacturing files (what the manufacturer uses to make the boards)
- **BOM (Bill of Materials)** - Component list with JLCPCB part numbers
- **Pick-and-Place** - Component placement coordinates for SMT assembly
- **EasyEDA project files** - If you want to modify the design

**Using these files:**
The Gerber, BOM, and pick-and-place files are what you'll upload to JLCPCB (or any other manufacturer). The schematic is there for reference if you want to understand how it works or make modifications.

If you're ordering from JLCPCB, their website walks you through the process pretty clearly. You'll need to review the component availability—sometimes parts go out of stock, and you might need to find substitutes.

---

## Related Documentation

**For building your first converter:**
- **[Hardware Overview](README.md)** - General hardware guide and component selection
- **[Getting Started Guide](../getting-started/hardware-setup.md)** - Breadboard wiring for testing

**Keyboard-specific:**
- Source code configuration: [`src/keyboards/modelf/pcat/`](../../src/keyboards/modelf/pcat/)

**External resources:**
- [EasyEDA](https://easyeda.com/) - Free PCB design tool (what I used)
- [JLCPCB](https://jlcpcb.com/) - PCB manufacturer with SMT assembly
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf) - If you want to modify the design

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
