# RP2040 Keyboard Converter for Raspberry Pi PICO RP2040

- [Custom Hardware Design](#custom-hardware-design)
    - [PCB Trace Design (Top)](#pcb-trace-design-top)
    - [PCB Trace Design (Bottom)](#pcb-trace-design-bottom)
    - [2D PCB Render (Top)](#2d-pcb-render-top)
    - [2D PCB Render (Bottom)](#2d-pcb-render-bottom)
    - [3D PCB Render (Top)](#3d-pcb-render-top)
    - [3D PCB Render (Bottom)](#3d-pcb-render-bottom)
    - [Manufacturerd PCBs](#manufacturerd-pcbs)
    - [Installed Converter](#installed-converter)
    - [Fully Assembled](#fully-assembled)
- [QMK Compatibility (Experimental)](#qmk-compatibility-experimental)


## Custom Hardware Design

As this project progressed, one key aspect which I wanted to achieve was my own custom interface converter which would fit inside the Model F PC/AT Keyboard, allowing me to keep to the original aesthetic of the keyboard, but not require a bulky external converter, or an excess amount of cable on the desk.

There have so far been 2 revisions of the design, Rev 1 was more of a test-fit PCB to ensure measurements were correct, and parts were poorly placed.  Rev 2 (current) is more of a final design, and is what I'm currently running in my PC/AT Keyboard.

This was designed entirely in EasyEDA (Standard Edition) and manufacturerd by JLCPCB using their SMT Assembly support.

Here you can find the [PCB Schematic](Schematic.pdf) for which the PCB Design is based off.

All Fabrication files (BOM, PickAndPlace and Gerber files) can be found [Here](fabrication).

#### PCB Trace Design (Top)
![PCB-Diag-Top](PCB-Diag-Top.png)

#### PCB Trace Design (Bottom)
![PCB-Diag-Bottom](PCB-Diag-Bottom.png)

#### 2D PCB Render (Top)
![2D-PCB-Top](2D-PCB-Top.png)

#### 2D PCB Render (Bottom)
![2D-PCB-Bottom](2D-PCB-Bottom.png)

#### 3D PCB Render (Top)
![3D-PCB-Top](3D-PCB-Top.png)

#### 3D PCB Render (Bottom)
![3D-PCB-Bottom](3D-PCB-Bottom.png)

#### Manufacturerd PCBs
![Manufactured-PCBs](jlcpcb-boards.png)

#### Installed Converter
![Installed PCB](installed-board.png)

#### Fully Assembled
![Assembled Board](assembled_6450225_1.png)

## QMK Compatibility (Experimental)
Even though this converter was not designed for use with QMK, it is compatible with work done by Markus Fritsche to enable RP2040 support in QMK which can be found [here](https://github.com/marfrit/qmk_firmware/tree/pio_ps2_converter/keyboards/converter/pio_ps2_converter)

To allow this to work, the file [info.json](https://github.com/marfrit/qmk_firmware/blob/pio_ps2_converter/keyboards/converter/pio_ps2_converter/info.json) must be edited to ensure the correct GPIO Pins are used, which match those used in this design:

```
    "ps2": {
        "enabled": true,
        "driver": "vendor",
        "clock_pin": "GP17",
        "data_pin": "GP16"
    },
```
