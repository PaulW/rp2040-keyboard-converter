# Keyboard Configuration and Layout Definitions

Within this folder you will find all supported Keyboards for this converter.  This includes Layout configurations and also protocol-specific changes.

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

* Please refer to [Protocols](/src/protocols) for a complete list of currently supported Protocols.

*Any extra options specified here are not yet supported.*