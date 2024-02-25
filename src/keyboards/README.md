# Keyboard Configuration and Layout Definitions

Within this folder you will find all supported Keyboards for this converter.  This includes Layout configurations and also protocol-specific changes.

Keyboards are selected at compilation time, and to use a different keyboard or layout, the firmware will need to be built with that specific keyboard specified.

Each keyboard consists of (at a minumum) 3 files:

| File | Description |
|---|---|
| keyboard.config | Configuration options for this Keyboard. |
| keyboard.c | Layout definitions for this Keyboard. |
| keyboard.h | Header file for the above |

## keyboard.config

This file defines specific Configuration options for this keyboard.  All of the following values are required, and the build will fail if they are missing or reference invalid items:

| Option | Description |
|---|---|
| MAKE | (string) Defines Manufacturer of this Keyboard |
| MODEL | (string) Specific Model Number for this Keyboard |
| DESCRIPTION | (string) Used to describe this keyboard |
| PROTOCOL | (string) Defines which Protocol this keyboard uses* |
| CODESET | (string) Defines which Codeset is in use with this Keyboard* |

* Please refer to [Protocols](/src/protocols/) for a complete list of currently supported Protocols.
* Please refer to [Scancodes](/src/scancodes/) for a complete list of currently supported Scancodes.

*Any extra options specified here are not yet supported.*