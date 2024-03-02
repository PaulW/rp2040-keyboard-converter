# IBM Model M Enhanced Keyboard (101-102-key)

![Model M (1390131)](doc/IBM-1390131.jpeg)

This is the Keyboard Mapping & Configuration for the IBM Model M Enhanced Keyboard, and covers both 101 and 102 key variants (ANSI and ISO).  It operates using the AT Protocol, and uses Set 2 Scancodes.

## Key Mapping

The keys can be re-assigned by updating the [keyboard.c](keyboard.c) file.  Please refer to [hid_keycodes.h](/src/common/lib/hid_keycodes.h) to list available key codes which can be mapped.  The Layers are defined and laid out in a way which matches the default key layout of the IBM Model M Keyboard.  I do intend on slightly updating the layout, and this will be made clear when I commit that change.

Please note, that some keys require the use of the Fn Modifier Key to be pressed (by default, this is mapped to Right-Alt).  Keys mapped with dual values also represent pressing Shift Modifier.

Right-Ctrl is mapped to LGUI (Windows Key or Command Key)

| Key on Keyboard | Modifier Mapping |
|---|---|
| CapsLock | Menu |

_* Mapping may differ on Windows PC, I've not tested this_

All Layouts are set as if the keyboard is set to British PC (as per my Mac)

![Layout Toggle](doc/layout-mac.png)