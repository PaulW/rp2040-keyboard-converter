# Model-F 5170 Converter for Raspberry Pi PICO RP2040

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

## Licence

The project is licensed under **GPLv3** or later. [Pico-SDK](https://github.com/raspberrypi/pico-sdk) and [TinyUSB](https://github.com/hathach/tinyusb) stack have their own license respectively, and as such remain intact in any included portions of code from those shared resources.

Ringbuffer implementation is from the official [TMK](https://github.com/tmk/tmk_keyboard) repository.

## Key Mapping

The current build has the following keys available when using the keyboard:
![Available Keys](doc/mapped-keys.png)
(Key Test performed using [QMK Configurator](https://config.qmk.fm/#/test))

The keys can be re-assigned by updating the [hid_keymaps.c](src/hid_keymaps.c) file.  Please refer to [hid_keycodes.h](src/hid_keycodes.h) to list available key codes which can be mapped.  The Layers are defined and laid out in a way which matches the default key layout of the IBM 5170 Keyboard.  I do intend on slightly updating the layout, and this will be made clear when I commit that change.

Please note, that some keys require the use of the Fn Modifier Key to be pressed (by default, this is mapped to F9).  Keys mapped with dual values also represent pressing Shift Modifier.

| Key on Keyboard | Modifier Mapping |
|---|---|
| Pipe / Back Slash | Grave / Negation * |
| Numpad 0 | Insert |
| Numpad Dot | Del |
| Numpad 1 | End |
| Numpad 2 | Down Arrow |
| Numpad 3 | Page Down |
| Numpad 4 | Left Arrow |
| Numpad 6 | Right Arrow |
| Numpad 7 | Home |
| Numpad 8 | Up Arrow |
| Numpad 9 | Page Up |
| F1 | F9 |
| F2 | F10 |
| F3 | F11 ** |
| F4 | F12 |
| F5 | Vol Down |
| F6 | Vol Up |
| F7 | Brightness Down |
| F8 | Brightness Up |

_* Mapping may differ on Windows PC, I've not tested this_

_** F11 Does work, it just doesn't show when pressed in Chrome on the QMK Congigurator Test Page_

All Layouts are set as if the keyboard is set to British PC (as per my Mac)

![Layout Toggle](doc/layout-mac.png)

## Building

Docker is used to perform the build tasks for this project, so to ensure a consistent build environment each time.

### Set up the Build Environment

To set up the Build Environment, we need to tell Docker to build a local container which is configured with the relevant build libraries and tools:

`docker-compose build builder`

### Building the Firmware

Next, we tell docker to run the container we have just built:

`docker-compose run builder`

This will build `ibm-5170-pcat.uf2` firmware file which you can then flash to your RP2040.  This file is located in the `./build` folder within your locally cloned repository.

### Flashing / Updating Firmware

Please refer to the relevant documentation for your Raspberry Pi Pico device.  However, as is commonly performed across multiple RP2040 controllers, the following steps should apply:
1. Put the RP2040 into Bootloader mode by holding BOOT and pressing RESET.  This should now mount the RP2040 as a volume named `RPI-RP2`.
2. Copy `build/ibm-5170-pcat.uf2`to the newly mounted volume.  Once copied, the volume will automatically unmount and the RP2040 will reboot.

The Firmware (once flashed) has the ability to put itself into Bootloader Mode by using a Macro Combination:

Press (and hold in order) - **Fn** + **LShift** + **RShift** + **B**

### Validating/Testing

The RP2040 should now perform an init task against the connected PC/AT Keyboard, and also present itself as a USB HID Device with 2 Interfaces:

```
$ lsusb -v
Bus 002 Device 002: ID 7e57:4008
Device Descriptor:
  bLength                18
  bDescriptorType         1
  bcdUSB               2.00
  bDeviceClass            0 (Defined at Interface level)
  bDeviceSubClass         0
  bDeviceProtocol         0
  bMaxPacketSize0        64
  idVendor           0x7e57
  idProduct          0x4008
  bcdDevice            1.00
  iManufacturer           1 IBM
  iProduct                2 Model F (PC/AT) Keyboard
  iSerial                 3 030323
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
        bInterval              10
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
        bInterval              10
Device Status:     0x0002
  (Bus Powered)
  Remote Wakeup Enabled
```
