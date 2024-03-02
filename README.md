# RP2040 Keyboard Converter for Raspberry Pi PICO RP2040
[![Test Building of Converter Firmware](https://github.com/PaulW/rp2040-keyboard-converter/actions/workflows/docker-image.yml/badge.svg)](https://github.com/PaulW/rp2040-keyboard-converter/actions/workflows/docker-image.yml)

This project is part of a refurbish/overhaul of an IBM Model F 5170 PC/AT Keyboard, specifically model 6450225. The goal of this is to allow me to use the Model F Keyboard on modern hardware via USB, but to have specific customisations where required.

Currently, the majority of converters available for this keyboard are based on Atmel AVR and Cortex-M hardware (such as TMK/QMK/VIAL, Soarers Converter to name a few), however with the attractive price point of RP2040-based hardware and availability, I decided I'd like to try and build my own converter from scratch, with the prospect of also creating custom interface hardware to build into the keyboard itself.

I am aware there is work on other converters which utilise a complete replacement of the controller board within the keyboard itself, however, I'd rather avoid physical hardware changes such as this, to allow me (if required) to restore the keyboard to its original state at some point in the future.

I am also deep-diving into learning about the RP2040, pico-sdk and tinyusb implementation, as it's been a good few years since I've last touched C.

## Current Hardware Requirements

Initially, code and builds were being tested using the following hardware setup:
- [WaveShare RP2040-Zero](https://www.waveshare.com/RP2040-Zero.htm)
- [4-channel I2C-safe Bi-directional Logic Level Converter - BSS138](https://www.adafruit.com/product/757)

This is connected as follows:
![alt text](doc/breadboard-schematic.png)

I have since designed a custom hardware solution which fits inside the Model F PC/AT Keyboard.  To simplify the documentation, I've kept the specific hardware details [Here](doc/custom_pcb.md)

I am also working on an inline connector design now, to allow support for multiple keyboards & protocols.

## Supported Keyboards

Please refer to the [Keyboards Folder](src/keyboards/) to see what current Keyboards are available and supported.  I plan on adding more as/when I get them to develop with, but please feel free to add your own.

## Supported Protocols

Currently, only the AT and XT Protocols are supported. As extra keyboards are added, more protocols will be supported in future.  Please refer to the [Protocols](src/protocols/) subfolder for more information.

## Supported Scancodes

Scancodes are sent from the keyboard to the host system allowing the hose to interpret the code for the key being pressed.  Standard set Scancodes (such as Set 1 and Set 2) as used on AT and XT Keyboards are currently implemented.  Support for other Scancodes will be added as/when other keyboards are added to the supported list.  Please refer to the [Scancodes](src/scancodes/) subfolder for more information.

## Building

Docker is used to perform the build tasks for this project, so to ensure a consistent build environment each time.

### Set up the Build Environment

To set up the Build Environment, we need to tell Docker to build a local container which is configured with the relevant build libraries and tools:

`docker-compose build builder`

### Building the Firmware

Next, we tell docker to run the container we have just built, and ensure that in the command line, we specify the specific Keyboard we wish to compile:

`docker-compose run -e KEYBOARD="modelf/6450225" builder`

In this example, we are specifying `-e KEYBOARD="modelf/6450225"` to build the Firmware with support for the IBM Model F Keyboard.  As new Keyboards are added (at time of writing there are 2), you simply specify the path from within the `keyboards` subfolder within `src`.

This will then build `rp2040-kbd-converter.uf2` firmware file which you can then flash to your RP2040.  This file is located in the `./build` folder within your locally cloned repository.

### Flashing / Updating Firmware

Please refer to the relevant documentation for your Raspberry Pi Pico device.  However, as is commonly performed across multiple RP2040 controllers, the following steps should apply:
1. Put the RP2040 into Bootloader mode by holding BOOT and pressing RESET.  This should now mount the RP2040 as a volume named `RPI-RP2`.
2. Copy `build/rp2040-kbd-converter.uf2`to the newly mounted volume.  Once copied, the volume will automatically unmount and the RP2040 will reboot.

The Firmware (once flashed) has the ability to put itself into Bootloader Mode by using a Macro Combination:

Press (and hold in order) - **Fn** + **LShift** + **RShift** + **B**

### Validating/Testing

The RP2040 should now perform an init task against the connected PC/AT Keyboard, and also present itself as a USB HID Device with 2 Interfaces:

```
$ lsusb -v
Bus 002 Device 002: ID 5515:4008
Device Descriptor:
  bLength                18
  bDescriptorType         1
  bcdUSB               1.10
  bDeviceClass            0 (Defined at Interface level)
  bDeviceSubClass         0
  bDeviceProtocol         0
  bMaxPacketSize0        64
  idVendor           0x5515
  idProduct          0x4008
  bcdDevice            1.00
  iManufacturer           1 paulbramhall.uk
  iProduct                2 RP2040 Keyboard Converter
  iSerial                 3 E66160F423782037
  bNumConfigurations      1
  Configuration Descriptor:
    bLength                 9
    bDescriptorType         2
    wTotalLength           59
    bNumInterfaces          2
    bConfigurationValue     1
    iConfiguration          0
    bmAttributes         0xa0
      (Bus Powered)
      Remote Wakeup
    MaxPower              300mA
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
Device Status:     0x0002
  (Bus Powered)
  Remote Wakeup Enabled
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

## Licence

The project is licensed under **GPLv3** or later. [Pico-SDK](https://github.com/raspberrypi/pico-sdk) and [TinyUSB](https://github.com/hathach/tinyusb) stack have their own license respectively, and as such remain intact in any included portions of code from those shared resources.

Ringbuffer implementation is on the one from the [TMK](https://github.com/tmk/tmk_keyboard) repository.