# VS Code Configuration Examples

This directory contains example VS Code configuration files for local development with IntelliSense and CMake integration.

## Setup for Local Development (Optional)

**Note:** Local development setup is **optional**. You can use Docker for all builds without any local tooling.

### Prerequisites

#### macOS

1. **CMake** (for IntelliSense and build configuration)
   ```bash
   brew install cmake
   ```

2. **ARM GCC Toolchain** (for compiling)
   ```bash
   brew install --cask gcc-arm-embedded
   ```
   - Installs to: `/Applications/ArmGNUToolchain/*/arm-none-eabi/bin/`
   - Symlinked to: `/opt/homebrew/bin/arm-none-eabi-gcc`

3. **Pico SDK Submodule** (already included if you cloned with `--recurse-submodules`)
   ```bash
   git submodule update --init --recursive
   ```

#### Linux

1. **CMake**
   ```bash
   # Debian/Ubuntu
   sudo apt-get install cmake
   
   # Fedora
   sudo dnf install cmake
   
   # Arch
   sudo pacman -S cmake
   ```

2. **ARM GCC Toolchain**
   ```bash
   # Debian/Ubuntu
   sudo apt-get install gcc-arm-none-eabi
   
   # Fedora
   sudo dnf install arm-none-eabi-gcc-cs
   
   # Arch
   sudo pacman -S arm-none-eabi-gcc
   ```
   - Typically installs to: `/usr/bin/arm-none-eabi-gcc`

3. **Pico SDK Submodule**
   ```bash
   git submodule update --init --recursive
   ```

#### Windows

1. **CMake**
   - Download from [cmake.org](https://cmake.org/download/)
   - Or use: `winget install Kitware.CMake`

2. **ARM GCC Toolchain**
   - Download from [ARM Developer](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)
   - Typically installs to: `C:\Program Files (x86)\GNU Arm Embedded Toolchain\<version>\bin\arm-none-eabi-gcc.exe`

3. **Pico SDK Submodule**
   ```bash
   git submodule update --init --recursive
   ```

### Installation

1. Copy the example configuration files to `.vscode/`:
   ```bash
   cp -r .vscode.example/ .vscode/
   ```

2. Configure CMake to generate `compile_commands.json`:
   ```bash
   mkdir -p .build-cache
   cd .build-cache
   cmake -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
         -DKEYBOARD=modelm/enhanced \
         -DMOUSE=at-ps2 \
         ../src
   cp compile_commands.json ..
   ```

3. Reload VS Code:
   - Press `Cmd+Shift+P` (Mac) or `Ctrl+Shift+P` (Windows/Linux)
   - Run **"Developer: Reload Window"**

### What You Get

- ✅ **IntelliSense** - Autocomplete for Pico SDK functions
- ✅ **Go to Definition** - Navigate to SDK headers and implementation
- ✅ **Error Checking** - Real-time syntax and type checking
- ✅ **Symbol Navigation** - Jump between functions and definitions
- ✅ **CMake Integration** - Configure and build from VS Code

### Customization

Edit `.vscode/settings.json` to change the keyboard/mouse configuration:

```json
"cmake.configureEnvironment": {
  "KEYBOARD": "your-keyboard-here",
  "MOUSE": "at-ps2"
}
```

Available keyboards: `modelm/enhanced`, `modelf/pcat`, `xt/ibm5150`, `apple/m0110a`, `amiga/a500`

### Platform-Specific Notes

#### macOS
- ARM toolchain path: `/opt/homebrew/bin/arm-none-eabi-gcc` (Apple Silicon) or `/usr/local/bin/arm-none-eabi-gcc` (Intel)
- The example `c_cpp_properties.json` uses the Homebrew path
- If you installed the toolchain differently, update `compilerPath` in `.vscode/c_cpp_properties.json`

#### Linux
- ARM toolchain typically at: `/usr/bin/arm-none-eabi-gcc`
- Update `compilerPath` in `.vscode/c_cpp_properties.json` to match your installation
- Configuration name can be changed from "RP2040" to "Linux" if desired

#### Windows
- ARM toolchain path example: `C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10/bin/arm-none-eabi-gcc.exe`
- Use forward slashes (`/`) in paths, not backslashes
- Update `compilerPath` in `.vscode/c_cpp_properties.json` to match your installation
- Create a "Windows" configuration in `c_cpp_properties.json` if needed

### Notes

- `.vscode/` is in `.gitignore` - your settings won't be committed
- `compile_commands.json` is regenerated each time you run CMake
- Docker builds don't require any of this setup
- **Compiler path varies by platform**: Update `compilerPath` in `c_cpp_properties.json` to match your system
  - macOS (Homebrew): `/opt/homebrew/bin/arm-none-eabi-gcc`
  - Linux: `/usr/bin/arm-none-eabi-gcc`
  - Windows: `C:/Program Files (x86)/GNU Arm Embedded Toolchain/<version>/bin/arm-none-eabi-gcc.exe`
