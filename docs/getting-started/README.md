# Getting Started

This guide will focus on getting you up and running quickly with a standard PS/2 keyboard connected to an RP2040 board. We'll try and keep things as simple yet comprehensive as possible.

## Why Convert Peripherals?

In this day and age, peripherals such as keyboards and mice are more of a throw-away item than something people keep for a long time. The overall quality and build of newer devices can feel cheap and flimsy compared to older hardware. I mean, yes, you can still get pretty high quality mechanical keyboards and mice today, but these come at a bit of a premium price, or might not be to everyone's taste.

I've always felt that old school keyboards and mice have a certain charm and quality about them. The way they feel to use—even older membrane keyboards feel much more robust and solid compared to a lot of modern equivalents (I weirdly like the feel of my Alps CL18821). The plastics are denser and thicker, and overall they just feel like they were built to last.

Also, with the popularity of older computers and retro gaming, there's a growing community of enthusiasts who appreciate the tech and want to use it with their modern systems.

## What Makes This Project Different? A Bit of History...

Other converters that allow you to use these devices with modern computers tend to be based around AVR microcontrollers (like the ATmega32U4 used in Arduino Pro Micro boards). Now these already have a very strong community and support around them, and there are some really great projects out there already using these microcontrollers too (such as TMK, QMK, and Soarer's Converter). However, I didn't have one of these to hand when I started this project (and I was already looking into a useful idea to use an RP2040), so my curiosity got the better of me. With finding an old IBM Model F Keyboard in my parents loft, I thought it would be a fun challenge to see if I could build something using an RP2040 board instead (I wanted to learn more about how the PIO state machines worked, having already played about with the Raspberry Pi and its GPIO capabilities previously).

When I first started to look into building something to use my keyboard, the examples I could find were Atmel based, or using other controllers such as a Teensy. I did however then come across an article on Adafruit '[IBM PC Keyboard to USB HID with CircuitPython](https://learn.adafruit.com/ibm-pc-keyboard-to-usb-hid-with-circuitpython/overview)', and this was an invaluable resource to help me get started testing whether the keyboard I had still worked. Now I already had experience using Python from my job, so to start with this was a nice and easy way to understand how things worked, and also a really good guide to help me understand more about how to connect things up.

The main problem though for this, is that it wasn't really a simple 'plug-and-play' solution. The Adafruit example was designed for an XT Keyboard, and the one I had was a PC/AT (so a completely different protocol). Signalling was somewhat similar in how it worked, I just needed to adapt it slightly to handle the differences of the AT/PS2 protocol compared to that of the XT protocol. So for simply reading the data being sent from the keyboard, this was a great starting point.

The only thing I wasn't a fan of with this approach though, is that even though CircuitPython is great for easily prototyping things and getting them off the ground quickly, I felt it wasn't as optimal an approach for something I'd like to use long-term. So I decided to look into building something more from the ground up in C using the official Raspberry Pi Pico SDK. As (at the time) there wasn't really any existing projects or examples out there working in a way I was looking for, I decided to build my own firmware from scratch.

Now, there has since been work going on to support platforms such as QMK on the RP2040 (Markus Fritsche for example has been doing some great work [here](https://github.com/marfrit/qmk_firmware/tree/pio_m4_converter/keyboards/converter/pio_ps2_converter)). This started around the same time I did this project, and it's been really interesting to see how that's evolved over time too! So credit to Markus and others who are working on that. I'm certainly not trying to compete with that, or re-invent the wheel—I just wanted to build something that suited my own needs and share it with others who might find it useful.

Over time, this project's evolved to support more protocols, allowing both a keyboard and mouse to be connected simultaneously, and has various features I felt would just make using it a bit handier overall (at least for my own use-cases). I've tried to document everything as best I can, so hopefully others can find it useful too.

## Quick Start

Pick where you want to start based on what you're trying to do:

### Connecting Your Device to an RP2040 Board
Start with the **[Hardware Setup Guide](hardware-setup.md)**.

This guide walks you through the basics of wiring your device to a RP2040 board step-by-step. You'll see exactly what parts you need, how to connect everything on a breadboard, and how to verify your setup works before moving on to firmware. If you want more detailed technical explanations about different voltage levels and component selection, check out the more in-depth [Hardware Documentation](../hardware/README.md).

### Compiling Firmware for Your Device
This guide explains the Docker-based build process I use, which gives you a consistent environment no matter what operating system you're using. You'll learn how to specify your device configuration, enable optional features like simultaneous keyboard and mouse support, and understand what the build produces. The Docker approach means you don't have to mess around with ARM compilers, CMake, or SDK versions—it's all handled for you.

**You'll need:**
- [Docker Desktop](https://www.docker.com/products/docker-desktop)
- [Git CLI](https://github.com/git-guides/install-git)

Once you have the base requirements installed, and you're ready to build the firmware, follow the **[Building Firmware Guide](building-firmware.md)**.  I've tried to keep this as simple as possible.

### Installing Firmware on Your RP2040
Once you've successfully built the firmware, follow the **[Flashing Firmware Guide](flashing-firmware.md)**. This will guide you through getting the firmware onto your RP2040 using a simple drag-and-drop process, how to use Command Mode for later firmware updates (so you don't need to physically press any buttons on the RP2040, as long as you've already built and flashed it once), and some troubleshooting steps if something doesn't work as expected.

Once the firmware's installed, you should be good to go! The board will reboot itself automatically (assuming everything went well), and it should now identify itself as a standard USB keyboard (and mouse if you've enabled that too). As long as you correctly configured the build time settings and your device is properly wired up, it should start working pretty much straight away.

---

## Need Help?

If you run into any issues or have questions, don't hesitate to reach out:

-  **[GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)** - Feel free to ask questions, share your build, or discuss features
- 🐛 **[GitHub Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)** - If you think you've found a bug or have hardware problems, log them here

**Before asking for help though, it'd be great if you could:**
1. Search through the existing discussions and issues—there's a good chance your question might have already been answered
2. Provide as much detail as you can: your keyboard model, what type of RP2040 board you're using, exact error messages, and photos of your wiring if it's relevant (pictures really do help!)

---

**[Next: Hardware Setup →](hardware-setup.md)**