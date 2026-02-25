# Building Firmware

**Time Required**: 10-15 minutes (first build)  
**Difficulty**: Beginner-friendly

This guide will walk you through building custom firmware for your specific keyboard and mouse combination. I've set things up to use Docker, which creates a consistent build environment that works the same way on Windows, macOS, and Linux—so you won't need to manually install compilers, SDKs, or development tools.

**What you'll accomplish:**
- Set up a complete build environment in a Docker container
- Compile firmware tailored to your specific keyboard model
- Generate a `.uf2` firmware file ready to flash to your RP2040 board

**Why Docker?** It packages all the complex build tools (ARM GCC compiler, Pico SDK, CMake) into a single container. You run one command, and Docker handles everything behind the scenes. This eliminates the "it works on my machine" problem and ensures everyone gets the same reliable build process.

---

## Prerequisites

Before you can build the firmware, you'll need to have:

- **Docker Desktop** installed and running
  - Windows: [Docker Desktop for Windows](https://docs.docker.com/desktop/install/windows-install/)
  - macOS: [Docker Desktop for Mac](https://docs.docker.com/desktop/install/mac-install/)
  - Linux: [Docker Engine](https://docs.docker.com/engine/install/)
- **Git** (to clone the repository)
- **10 GB free disk space** (for Docker images and build artifacts)

**Note**: The Docker container includes all the required build tools (ARM GCC toolchain, Pico SDK, CMake, etc.), so you don't need to install these separately.

---

## Step 1: Clone the Repository

First, download the project source code to your computer. Open a terminal (macOS/Linux) or PowerShell (Windows) and run:

```bash
git clone https://github.com/PaulW/rp2040-keyboard-converter.git
cd rp2040-keyboard-converter
```

**What's happening here:**
- `git clone` downloads the project from GitHub to your computer
- `cd rp2040-keyboard-converter` moves you into the project directory

That's it! The Docker container includes the Pico SDK, so you don't need to download it separately.

**Note for developers:** If you're planning to do local development work (editing code with IDE autocomplete, debugging, that sort of thing), you'll want the Pico SDK locally for your development tools. In that case, clone with `--recurse-submodules`:

```bash
git clone --recurse-submodules https://github.com/PaulW/rp2040-keyboard-converter.git
```

Or add it after cloning:

```bash
git submodule update --init --recursive
```

For just building firmware, this isn't necessary—Docker has everything it needs.

---

## Step 2: Build the Docker Container

Now we'll create the Docker container with all the build tools. This is a one-time setup—you'll only do this once (unless we update the build environment), so whilst it might take a few minutes, you won't have to do it again:

```bash
docker compose build builder
```

**What's happening behind the scenes:**
1. Docker downloads a base Ubuntu Linux image
2. Installs the ARM GCC cross-compiler (converts code to RP2040-compatible binaries)
3. Installs the Pico SDK version 2.2.0 (Raspberry Pi's official libraries)
4. Configures CMake (manages the build process)
5. Sets up all necessary dependencies

**Duration**: 5-10 minutes on first run (depends on your internet speed)

**Important**: This creates a cached Docker image. Once it's built, it stays on your computer and gets reused for all future firmware builds. You won't wait this long again unless you need to rebuild the container (which is pretty rare).

---

## Step 3: Choose Your Configuration

Before building, you'll need to tell the build system which keyboard model you're using. Each keyboard has its own configuration that includes:
- **Protocol**: The communication method (AT/PS2, XT, Amiga, M0110)
- **Scancode set**: How the keyboard reports keypresses
- **Layout**: Physical key arrangement and mapping

### Finding Your Keyboard

Browse the available keyboard configurations in the **[Supported Keyboards](../keyboards/README.md)** documentation. Each keyboard has a configuration path like `modelm/enhanced` or `amiga/a500`.

You can also list all available keyboards from the terminal:

```bash
find src/keyboards -name "keyboard.config"
```

This shows paths like:
```
src/keyboards/modelm/enhanced/keyboard.config
src/keyboards/amiga/a500/keyboard.config
src/keyboards/apple/m0110a/keyboard.config
```

The configuration path is the part after `src/keyboards/`—for example, `modelm/enhanced`.

**Not sure which configuration to use?** The `modelm/enhanced` configuration works as a general-purpose AT/PS2 configuration for standard 101/104-key layouts.

For keyboards with specific layouts or different protocols (XT, Amiga, M0110), have a look at the [Supported Keyboards](../keyboards/README.md) documentation to find the configuration that best matches your hardware.

### Mouse Support (Optional)

The converter can also support a mouse alongside your keyboard if you want. This is completely optional.

**If you want mouse support:**
- The converter supports AT/PS2 mouse protocol
- Requires separate hardware wiring (the mouse uses its own CLOCK/DATA pins—see [Hardware Setup](hardware-setup.md))
- Works with any keyboard protocol (so you could use an AT/PS2 mouse with an Amiga keyboard, for example)

**For this getting started guide**, we'll focus on keyboard-only builds. If you do need mouse support, the process is nearly identical—you'd just add `-e MOUSE="at-ps2"` to your build command. Have a look at the [Mouse Support documentation](../features/README.md#mouse-support) for the details.

---

## Step 4: Build Firmware

Right, now for the fun part—actually compiling the firmware! The build process is pretty simple: you tell Docker which keyboard you're using, and it handles all the compilation details.

### The Build Command

Run this command, replacing `modelm/enhanced` with your keyboard's configuration path:

```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

**Breaking down the command:**
- `docker compose run` - Starts a Docker container from the image we built earlier
- `--rm` - Automatically removes the container when done (keeps your system clean)
- `-e KEYBOARD="modelm/enhanced"` - Sets an environment variable telling the build system which keyboard to configure for
- `builder` - The name of our build container (defined in [`docker-compose.yml`](../../docker-compose.yml))

**That's it!** This single command triggers the entire build process.

### What Happens During the Build

When you run the build command, Docker orchestrates quite a complex series of steps behind the scenes. You don't need to understand all the details, but here's what's happening:

1. **Container starts** - Docker launches the build environment with all the tools ready
2. **Configuration loads** - CMake reads your keyboard's `keyboard.config` file
3. **Dependencies resolve** - The build system works out which source files are needed
4. **Protocol handler includes** - Adds the correct protocol code (AT/PS2, XT, Amiga, or M0110)
5. **Scancode processor includes** - Adds scancode translation for your keyboard's specific format
6. **Keymap includes** - Loads your keyboard's layout and key mappings
7. **PIO compilation** - Converts PIO assembly (hardware timing programs) into C headers
8. **Source compilation** - ARM GCC compiles all the C files into machine code for the RP2040
9. **Linking** - Combines everything into a single executable
10. **UF2 generation** - Creates the flashable firmware file
11. **Cleanup** - Container exits and removes itself

**Duration**: 30-60 seconds (after the Docker image's been built)

The beauty of this system is that it's completely automated—you don't need to know C programming, understand CMake syntax, or configure toolchains. Just specify your keyboard, and Docker handles the rest.

### Understanding Build Output

When the build completes successfully, you'll see output that looks something like this:

```
-- Build files have been written to: /build
Scanning dependencies of target rp2040-converter
[ 12%] Building C object CMakeFiles/rp2040-converter.dir/src/main.c.obj
[ 25%] Building C object CMakeFiles/rp2040-converter.dir/src/common/lib/hid_interface.c.obj
...
[100%] Linking CXX executable rp2040-converter.elf
   text    data     bss     dec     hex filename
 175432    2568   27144  205144   3213c rp2040-converter.elf
[100%] Built target rp2040-converter
```

**What those percentages mean**: Each source file gets compiled individually, and the percentages show progress through all the files. When it reaches 100%, everything's been compiled and linked together.

**Output files created in `./build/` directory:**
- **`rp2040-converter.uf2`** ← This is the file you'll flash to your RP2040
- `rp2040-converter.elf` - Binary with debugging symbols (for developers)
- `rp2040-converter.elf.map` - Memory layout map (for developers)

You only need the `.uf2` file for normal use—the others are for debugging and development.

---

## Step 5: Verify Build Output

Let's confirm the firmware file was created successfully. From your terminal, check the `build/` directory:

**On macOS/Linux:**

```bash
ls -lh build/rp2040-converter.uf2
```

**On Windows (PowerShell):**

```powershell
Get-ChildItem build\rp2040-converter.uf2
```

**Expected output:**
```
-rw-r--r--  1 user  staff   88K Oct 30 10:30 build/rp2040-converter.uf2
```

**What to look for:**
- File exists in the `build/` directory
- File size: 80-120 KB range (varies by configuration)
- Size depends on keyboard configuration, enabled features, and code changes

If you see the file with a reasonable size, brilliant! Your firmware's built and ready to flash.

---

## Understanding Build Configuration

The build system uses environment variables to customise the firmware. Let's break down what each one does:

### The KEYBOARD Variable (Required)

Every build needs a keyboard configuration. This is how you tell the build system which keyboard you're using:

```bash
-e KEYBOARD="<brand>/<model>"
```

**What this does:**
- Points to the configuration file at `src/keyboards/<brand>/<model>/keyboard.config`
- Loads the correct protocol handler (AT/PS2, XT, Amiga, etc.)
- Includes the appropriate scancode set (how keys are numbered)
- Loads the keyboard-specific layout and key mappings

**Real-world example**: When you build with `KEYBOARD="modelm/enhanced"`, the system:
- Reads [`src/keyboards/modelm/enhanced/keyboard.config`](../../src/keyboards/modelm/enhanced/keyboard.config)
- Sees it needs the AT/PS2 protocol
- Includes Scancode Set 1/2/3 unified processor (`set123`)
- Loads the 101-key Enhanced keyboard layout
- Compiles in the keymap from [`src/keyboards/modelm/enhanced/keyboard.c`](../../src/keyboards/modelm/enhanced/keyboard.c)

### The MOUSE Variable (Optional)

Add mouse support to your keyboard converter:

```bash
-e MOUSE="at-ps2"
```

**What this does:**
- Includes the AT/PS2 mouse protocol handler code
- Allocates a separate PIO state machine for mouse communication
- Configures GPIO 6/7 for mouse CLOCK/DATA (independent from keyboard pins - see [`src/config.h`](../../src/config.h))
- Enables simultaneous keyboard and mouse conversion

**Currently supported**: Only `at-ps2` mouse protocol (standard PS/2 mice)

**Note**: You can use an AT/PS2 mouse with any keyboard protocol—the mouse operates independently.

---

## Troubleshooting

Running into issues? Here are some troubleshooting steps:

---

### ❌ "When building, you need to ensure you set the required Keyboard or Mouse"

**What this means**: You need to specify at least one device—either a keyboard, a mouse, or both.

**How to fix it**:
You can build with any of these options:

**Keyboard only:**
```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

**Mouse only:**
```bash
docker compose run --rm -e MOUSE="at-ps2" builder
```

**Both keyboard and mouse:**
```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

---

### ❌ "File 'keyboard.config' does not exist!" or "keyboard.config not found"

**What this means**: The keyboard path you specified doesn't exist, or you have a typo somewhere.

**How to fix it**:
1. Check which keyboards are actually available:
   ```bash
   find src/keyboards -name "keyboard.config"
   ```

2. Use the path shown after `src/keyboards/`
   - ✅ Correct: `modelm/enhanced`
   - ❌ Wrong: `src/keyboards/modelm/enhanced`
   - ❌ Wrong: `ModelM/Enhanced` (case matters!)

---

### ❌ "Keyboard config file is missing the following key(s)"

**What this means**: The keyboard configuration file exists but is incomplete or corrupted.

**How to fix it**:
The keyboard configuration file may be missing required fields. Try:

1. Verify you're using an official keyboard configuration from the repository
2. If you created a custom keyboard, check your `keyboard.config` file includes all required fields:
   - `MAKE` - Keyboard manufacturer
   - `DESCRIPTION` - Keyboard description
   - `MODEL` - Keyboard model
   - `PROTOCOL` - Protocol handler (at-ps2, xt, amiga, m0110)
   - `CODESET` - Scancode set to use

3. Compare your config against a working example like [`src/keyboards/modelm/enhanced/keyboard.config`](../../src/keyboards/modelm/enhanced/keyboard.config)

---

### ❌ "Cannot connect to Docker daemon"

**What this means**: Docker isn't running on your computer.

**How to fix it**:
1. Open the Docker Desktop application
2. Wait until you see "Docker Desktop is running" (you'll see a green indicator)
3. Try your build command again

**Still not working?** Make sure Docker Desktop's actually installed. Have a look at the [Prerequisites](#prerequisites) section for installation links.

---

### ❌ Build succeeds, but no UF2 file appears

**What this means**: Docker's built the firmware, but it's not appearing in your `build/` folder. This may be a volume mounting issue.

**How to fix it**:
1. First, check if the file exists inside the Docker container:
   ```bash
   docker compose run --rm -e KEYBOARD="modelm/enhanced" builder ls -l /build
   ```

2. If you see the file listed, but it's not in your local `build/` folder, check your [`docker-compose.yml`](../../docker-compose.yml):
   ```yaml
   volumes:
     - ./build:/build
   ```
   This line should be present in the `builder` service definition.

3. Try removing the build directory and starting fresh:
   ```bash
   rm -rf build/
   mkdir build
   docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
   ```

---

### ❌ Permission denied (Linux only)

**What this means**: Docker's created files owned by a different user (UID 1001), and your user can't access them.

**How to fix it**:
Run this command from your project root directory:
```bash
sudo chown -R $USER:$USER build/
```

This changes ownership of all the files in `build/` to your user account.

**Note**: The Docker container runs as UID 1001 by default (defined in [`docker-compose.yml`](../../docker-compose.yml)). If this frequently causes issues on your system, you can modify the `user:` line in `docker-compose.yml` to match your user ID.

---

### Verify Docker Installation

If build issues persist:

1. **Check your Docker installation**:
   ```bash
   docker --version
   docker compose version
   ```
   You should see version numbers. If you get "command not found", Docker isn't installed correctly.

2. **Try a clean rebuild**:
   ```bash
   rm -rf build/*
   docker compose build builder --no-cache
   docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
   ```

3. **Ask for help**: Visit [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) with:
   - Your operating system (Windows, macOS, Linux)
   - The full error message
   - The keyboard configuration you're trying to build

---

## Advanced: Rebuilding After Changes

As you use your converter, you might want to rebuild the firmware after making changes or trying out different keyboard configurations.

### Rebuilding with the Same Configuration

If you modify source code or configuration files, just run the build command again:

```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

The build process runs from scratch each time—the Docker entrypoint script automatically cleans all build artifacts before compiling to ensure a fresh, consistent build every time.

**Duration**: 30-60 seconds

### Switching Keyboard Configurations

Want to try a different keyboard? Just change the `KEYBOARD` parameter:

```bash
docker compose run --rm -e KEYBOARD="amiga/a500" builder
```

The build system automatically includes the new keyboard's protocol, scancode set, and keymap. The output file (`rp2040-converter.uf2`) is overwritten with the new configuration.

**Note**: The Docker build process automatically cleans old build files before compiling, so you always get a fresh build.

### Updating Build Tools (Rare)

If the project updates its build dependencies or Pico SDK version, you'll need to rebuild the Docker image:

```bash
docker compose build builder --no-cache
```

The `--no-cache` flag forces Docker to rebuild from scratch, ignoring cached layers. This takes 5-10 minutes but ensures you have the latest build environment.

---

## Build System Details

### What Gets Included?

The build system automatically includes based on your configuration:

**Always included:**
- Core firmware ([`src/main.c`](../../src/main.c))
- USB HID interface ([`src/common/lib/hid_interface.c`](../../src/common/lib/hid_interface.c), [`hid_interface.h`](../../src/common/lib/hid_interface.h))
- Ring buffer implementation ([`src/common/lib/ringbuf.c`](../../src/common/lib/ringbuf.c), [`ringbuf.h`](../../src/common/lib/ringbuf.h))
- Command Mode ([`src/common/lib/command_mode.c`](../../src/common/lib/command_mode.c), [`command_mode.h`](../../src/common/lib/command_mode.h))
- TinyUSB stack (from Pico SDK)

**Configuration-dependent:**
- Protocol handler: [`src/protocols/`](../../src/protocols/)`<protocol>/` (from `keyboard.config`, see [Protocols](../protocols/README.md))
- Scancode processor: [`src/scancodes/`](../../src/scancodes/)`<set>/` (from `keyboard.config`, see [Scancode Sets](../scancodes/README.md))
- PIO programs: [`src/protocols/`](../../src/protocols/)`<protocol>/*.pio` (compiled to headers)
- Keyboard layout: [`src/keyboards/`](../../src/keyboards/)`<brand>/<model>/keyboard.c` (see [Keyboards](../keyboards/README.md))

### Build Outputs Explained

| File | Purpose | Size |
|------|---------|------|
| `rp2040-converter.uf2` | Flashable firmware (drag-and-drop) | ~80-120 KB |
| `rp2040-converter.elf` | Binary with debug symbols | ~1-2 MB |
| `rp2040-converter.elf.map` | Memory layout and symbol addresses | ~400-600 KB |

**For flashing**: You only need the `.uf2` file.

**For debugging**: Use `.elf` with GDB or ARM debugger.

**Note**: File sizes are approximate and vary based on keyboard configuration, compile-time options, and code changes.

---

## Next Steps

Now that you have compiled firmware, proceed to:

**→ [Flashing Firmware](flashing-firmware.md)** - Install firmware on your RP2040 board

**Additional Resources:**
- [Hardware Setup](hardware-setup.md) - Physical wiring guide
- [Supported Keyboards](../keyboards/README.md) - Keyboard-specific details
- [Development Guide](../development/README.md) - Adding new keyboards or protocols
- [Build System Architecture](../advanced/build-system.md) - Technical deep-dive

---

## Quick Reference

Once you're familiar with the process, here's the condensed version:

**First-time setup:**
```bash
git clone https://github.com/PaulW/rp2040-keyboard-converter.git
cd rp2040-keyboard-converter
docker compose build builder
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

**Rebuild after changes:**
```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

**Output location:**
```
./build/rp2040-converter.uf2
```

**Find available keyboards:**
```bash
find src/keyboards -name "keyboard.config"
```

---

**[← Previous: Hardware Setup](hardware-setup.md)** | **[Next: Flashing Firmware →](flashing-firmware.md)**
