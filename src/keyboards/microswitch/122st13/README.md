# MicroSwitch 122ST13 Keyboard

![G80-0614H](doc/G80-0614H.jpeg)

This is the Keyboard Mapping & Configuration for the MicroSwitch 122ST13.  There are multiple versions of this board, in particular (this use case) we are basing on the 122ST13-9E-J.

## Key Mapping

The keys can be re-assigned by updating the [keyboard.c](keyboard.c) file.  Please refer to [hid_keycodes.h](/src/common/lib/hid_keycodes.h) to list available key codes which can be mapped.

Even though we assign the Fn Modifier, this is done to allow us to still enter Bootloader mode for updating Firmware.

All Layouts are set as if the keyboard is set to British PC (as per my Mac)

![Layout Toggle](doc/layout-mac.png)