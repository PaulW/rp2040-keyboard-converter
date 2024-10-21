# RP2040 Keyboard & Mouse Converter for Raspberry Pi PICO RP2040
[![Test Building of Converter Firmware](https://github.com/PaulW/rp2040-keyboard-converter/actions/workflows/docker-image.yml/badge.svg)](https://github.com/PaulW/rp2040-keyboard-converter/actions/workflows/docker-image.yml)

This project originally was part of a refurbish/overhaul of an IBM Model F 5170 PC/AT Keyboard, specifically model 6450225. The goal of this was to allow me to use the Model F Keyboard on modern hardware via USB, but to have specific customisations where required.  Now that this has been completed, I've moved my attention on further enhancing the Firmware to support multiple keyboards and protocols, as well as inclusion of support for mice.

Currently, the majority of converters available are based on Atmel AVR and Cortex-M hardware (such as TMK/QMK/VIAL, Soarers Converter to name a few), however with the attractive price point of RP2040-based hardware and availability, this is why I decided I'd like to try and build my own converter from scratch, with the prospect of also creating custom interface hardware to build into the keyboard itself.

I am aware there is work on other converters which utilise a complete replacement of the controller board within the keyboard itself, however, I'd rather avoid physical hardware changes such as this, to allow me (if required) to restore the keyboard to its original state at some point in the future.

I am also deep-diving into learning about the RP2040, pico-sdk and tinyusb implementation, as it's been a good few years since I've last touched C.

## Current Hardware Requirements

Initially, code and builds were being tested using the following hardware setup:
- [WaveShare RP2040-Zero](https://www.waveshare.com/RP2040-Zero.htm)
- [4-channel I2C-safe Bi-directional Logic Level Converter - BSS138](https://www.adafruit.com/product/757)

This is connected as follows:
![alt text](doc/breadboard-schematic.png)

I have since designed a custom hardware solution which fits inside the Model F PC/AT Keyboard.  To simplify the documentation, I've kept the specific hardware details [Here](doc/custom_pcb.md)

I am also working on an inline connector design now, to allow support for multiple keyboards, mice & protocols.

## Supported Keyboards

Please refer to the [Keyboards Folder](src/keyboards/) to see what current Keyboards are available and supported.  I plan on adding more as/when I get them to develop with, but please feel free to add your own.

## Supported Mice

Mice support is built more directly into the Protocol spec than having individual configuration such as we do for Keyboards.  As such, when specifying a Mouse, you only need to specify the protocol as the option.

## Supported Protocols

Currently, only the AT and XT Protocols are supported. As extra keyboards are added, more protocols will be supported in future.  Please refer to the [Protocols](src/protocols/) subfolder for more information.

## Supported Scancodes

Scancodes are sent from the keyboard to the host system allowing the hose to interpret the code for the key being pressed.  Standard set Scancodes (such as Set 1 and Set 2) as used on AT and XT Keyboards are currently implemented.  Support for other Scancodes will be added as/when other keyboards are added to the supported list.  Please refer to the [Scancodes](src/scancodes/) subfolder for more information.

## Building

Docker is used to perform the build tasks for this project, so to ensure a consistent build environment each time.

### Set up the Build Environment

To set up the Build Environment, we need to tell Docker to build a local container which is configured with the relevant build libraries and tools:

`docker compose build builder`

### Building the Firmware

Next, we tell docker to run the container we have just built, and ensure that in the command line, we specify the specific Keyboard we wish to compile:

`docker compose run -e KEYBOARD="modelf/pcat" -e MOUSE="at-ps2" builder`

In this example, we are specifying `-e KEYBOARD="modelf/pcat"` to build the Firmware with support for the IBM Model F Keyboard.  We also specify `-e MOUSE="at-ps2"` which also tells the compiler to build Mouse support for this particular protocol too.  As new Keyboards and Mice are added, you simply specify the path from within the `keyboards` subfolder within `src`, as well as the relevant protocol required for the Mouse.

This will then build `rp2040-converter.uf2` firmware file which you can then flash to your RP2040.  This file is located in the `./build` folder within your locally cloned repository.

### Flashing / Updating Firmware

Please refer to the relevant documentation for your Raspberry Pi Pico device.  However, as is commonly performed across multiple RP2040 controllers, the following steps should apply:
1. Put the RP2040 into Bootloader mode by holding BOOT and pressing RESET.  This should now mount the RP2040 as a volume named `RPI-RP2`.
2. Copy `build/rp2040-converter.uf2`to the newly mounted volume.  Once copied, the volume will automatically unmount and the RP2040 will reboot.

The Firmware (once flashed) has the ability to put itself into Bootloader Mode by using a Macro Combination:

Press (and hold in order) - **Fn** + **LShift** + **RShift** + **B**

Please note, there is no Macro Combination for entering Bootloader mode when only a Mouse has been built, as such, you will need to manually hold the BOOT switch when powering on or pressing RESET.

### Validating/Testing
Here we see the output from `lsusb -v` for when the converter is configured for both Keyboard and Mouse support.  Please note, only specific configurations are defined depending on the required build-time options.  The converter will not identify as a device for something it has not been built for.
```
Bus 002 Device 001: ID 5515:400c
Device Descriptor:
  bLength                18
  bDescriptorType         1
  bcdUSB               1.10
  bDeviceClass            0 (Defined at Interface level)
  bDeviceSubClass         0
  bDeviceProtocol         0
  bMaxPacketSize0        64
  idVendor           0x5515
  idProduct          0x400c
  bcdDevice            1.00
  iManufacturer           1 paulbramhall.uk
  iProduct                2 RP2040 Device Converter
  iSerial                 3 E66160F423782037
  bNumConfigurations      1
  Configuration Descriptor:
    bLength                 9
    bDescriptorType         2
    wTotalLength           84
    bNumInterfaces          3
    bConfigurationValue     1
    iConfiguration          0
    bmAttributes         0x80
      (Bus Powered)
    MaxPower              250mA
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        0
      bAlternateSetting       0
      bNumEndpoints           1
      bInterfaceClass         3 Human Interface Device
      bInterfaceSubClass      1 Boot Interface Subclass
      bInterfaceProtocol      1 Keyboard
      iInterface              0
        HID Device Descriptor:
          bLength                 9
          bDescriptorType        33
          bcdHID               1.11
          bCountryCode            0 Not supported
          bNumDescriptors         1
          bDescriptorType        34 Report
          wDescriptorLength      67
         Report Descriptors:
           ** UNAVAILABLE **
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x81  EP 1 IN
        bmAttributes            3
          Transfer Type            Interrupt
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0008  1x 8 bytes
        bInterval               8
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        1
      bAlternateSetting       0
      bNumEndpoints           1
      bInterfaceClass         3 Human Interface Device
      bInterfaceSubClass      0 No Subclass
      bInterfaceProtocol      0 None
      iInterface              0
        HID Device Descriptor:
          bLength                 9
          bDescriptorType        33
          bcdHID               1.11
          bCountryCode            0 Not supported
          bNumDescriptors         1
          bDescriptorType        34 Report
          wDescriptorLength      25
         Report Descriptors:
           ** UNAVAILABLE **
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x82  EP 2 IN
        bmAttributes            3
          Transfer Type            Interrupt
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0010  1x 16 bytes
        bInterval               8
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        2
      bAlternateSetting       0
      bNumEndpoints           1
      bInterfaceClass         3 Human Interface Device
      bInterfaceSubClass      1 Boot Interface Subclass
      bInterfaceProtocol      2 Mouse
      iInterface              0
        HID Device Descriptor:
          bLength                 9
          bDescriptorType        33
          bcdHID               1.11
          bCountryCode            0 Not supported
          bNumDescriptors         1
          bDescriptorType        34 Report
          wDescriptorLength      79
         Report Descriptors:
           ** UNAVAILABLE **
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x83  EP 3 IN
        bmAttributes            3
          Transfer Type            Interrupt
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0040  1x 64 bytes
        bInterval               8
Device Status:     0x0000
  (Bus Powered)
```

## Serial Debugging

If you have a Serial-UART device connected to the UART output of the RP2040, then you will see some extra debugging information while the converter runs.  Here is an example output from when the RP2040 is powered on with a keyboard connected:

```
--------------------------------
[INFO] RP2040 Keyboard Converter
[INFO] RP2040 Serial ID: E66160F423782037
[INFO] Build Time: 2024-03-02 13:40
--------------------------------
[INFO] Keyboard Make: IBM
[INFO] Keyboard Model: 1390131
[INFO] Keyboard Description: Model M (101-key)
[INFO] Keyboard Protocol: at
[INFO] Keyboard Scancode Set: set2
--------------------------------
[INFO] RP2040 Clock Speed: 125000KHz
[INFO] Interface Polling Interval: 50us
[INFO] Interface Polling Clock: 20kHz
[INFO] Clock Divider based on 11 SM Cycles per Keyboard Clock Cycle: 568.00
[INFO] Effective SM Clock Speed: 220.07kHz
[INFO] PIO SM Interface program loaded at 7 with clock divider of 568.00
--------------------------------
[DBG] Waiting for Keyboard Clock HIGH
[DBG] Waiting for Keyboard Clock HIGH
[DBG] Keyboard Detected, waiting for ACK (1/5)
[DBG] Keyboard Detected, waiting for ACK (2/5)
[DBG] Keyboard Self Test OK!
[DBG] Waiting for Keyboard ID...
[DBG] Keyboard ID/Setup Timeout, retrying...
[DBG] ACK Keyboard ID Request
[DBG] Waiting for Keyboard ID...
[DBG] Keyboard First ID Byte read as 0xAB
[DBG] Keyboard Second ID Byte read as 0x83
[DBG] Keyboard ID: 0xAB83
[DBG] Keyboard Initialised!
```

## License

The project is licensed under **GPLv3** or later. Third-party libraries and code used in this project have their own licenses as follows:

* **Pico-SDK (https://github.com/raspberrypi/pico-sdk)**: License information can be found in the Pico-SDK repository.

* **TinyUSB (https://github.com/hathach/tinyusb)**: License information can be found in the TinyUSB repository. These licenses remain intact in any included portions of code from those shared resources.

* **Pico-Simon (https://github.com/Zheoni/pico-simon)**: Portions of code from the Pico-Simon project are used under the MIT License. A copy of the MIT License can be found in the Pico-Simon repository.

* **Ringbuffer implementation (source files: ringbuf.c, etc.)**:
  * Based on code originally created by Hasu@tmk for the TMK Keyboard Firmware project.
  * Potential source references:
    * https://github.com/tmk/tinyusb_ps2/blob/main/ringbuf.h
    * https://github.com/tmk/tmk_keyboard/blob/master/tmk_core/ring_buffer.h
  * License: Likely GPLv2 or later (same as TMK) due to its inclusion in the TMK repository, but the exact origin is unclear.
