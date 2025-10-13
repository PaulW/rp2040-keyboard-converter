#!/bin/sh

set -e

echo "Cleaning up old Build Files..."
rm -rf ${APP_DIR}/build/*
echo "Creating Temporary Build Location..."
mkdir -p ${APP_DIR}/build-tmp
cd ${APP_DIR}/build-tmp
echo "Cleaning old build/compile files (if any)..."
rm -rf cmake_install.cmake CMakeCache.txt CMakeFiles elf2uf2 generated Makefile pico-sdk pioasm
cmake ${APP_DIR}/src
cmake --build .

# Build Analysis
echo ""
echo "========================================"
echo "Build Analysis"
echo "========================================"
echo ""

# Check if build artifacts exist
if [ -f "${APP_DIR}/build/rp2040-converter.elf" ]; then
    echo "Build Output Files:"
    ls -lh ${APP_DIR}/build/rp2040-converter.* 2>/dev/null || echo "  Warning: Some build artifacts missing"
    echo ""
    
    echo "Memory Usage Analysis:"
    echo "----------------------"
    arm-none-eabi-size ${APP_DIR}/build/rp2040-converter.elf
    echo ""
    
    # More detailed breakdown
    echo "Detailed Section Sizes:"
    echo "-----------------------"
    arm-none-eabi-size -A -x ${APP_DIR}/build/rp2040-converter.elf | head -20
    echo ""
    
    # Calculate percentages for RP2040 (256KB Flash, 264KB RAM)
    FLASH_SIZE=262144  # 256KB in bytes
    RAM_SIZE=270336    # 264KB in bytes
    
    TEXT_SIZE=$(arm-none-eabi-size ${APP_DIR}/build/rp2040-converter.elf | tail -1 | awk '{print $1}')
    DATA_SIZE=$(arm-none-eabi-size ${APP_DIR}/build/rp2040-converter.elf | tail -1 | awk '{print $2}')
    BSS_SIZE=$(arm-none-eabi-size ${APP_DIR}/build/rp2040-converter.elf | tail -1 | awk '{print $3}')
    
    FLASH_USED=$((TEXT_SIZE + DATA_SIZE))
    RAM_USED=$((DATA_SIZE + BSS_SIZE))
    
    FLASH_PERCENT=$((FLASH_USED * 100 / FLASH_SIZE))
    RAM_PERCENT=$((RAM_USED * 100 / RAM_SIZE))
    
    echo "RP2040 Resource Usage:"
    echo "----------------------"
    echo "Flash: ${FLASH_USED} / ${FLASH_SIZE} bytes (${FLASH_PERCENT}%)"
    echo "RAM:   ${RAM_USED} / ${RAM_SIZE} bytes (${RAM_PERCENT}%)"
    echo ""
    
    # UF2 file info (use wc -c for portability)
    if [ -f "${APP_DIR}/build/rp2040-converter.uf2" ]; then
        UF2_SIZE=$(wc -c < "${APP_DIR}/build/rp2040-converter.uf2" | tr -d ' ')
        UF2_SIZE_KB=$((UF2_SIZE / 1024))
        echo "UF2 File Size: ${UF2_SIZE} bytes (${UF2_SIZE_KB} KB)"
    fi
    
    echo ""
    echo "Build Validation:"
    echo "-----------------"
    
    # Check for resource usage warnings
    if [ ${FLASH_PERCENT} -ge 90 ]; then
        echo "⚠️  WARNING: Flash usage is ${FLASH_PERCENT}% - approaching limit!"
    elif [ ${FLASH_PERCENT} -ge 75 ]; then
        echo "⚠️  NOTICE: Flash usage is ${FLASH_PERCENT}% - consider optimization"
    else
        echo "✓ Flash usage: ${FLASH_PERCENT}% (healthy)"
    fi
    
    if [ ${RAM_PERCENT} -ge 85 ]; then
        echo "⚠️  WARNING: RAM usage is ${RAM_PERCENT}% - risk of stack overflow!"
    elif [ ${RAM_PERCENT} -ge 70 ]; then
        echo "⚠️  NOTICE: RAM usage is ${RAM_PERCENT}% - monitor stack depth"
    else
        echo "✓ RAM usage: ${RAM_PERCENT}% (healthy)"
    fi
    
    # Check for binary info (picotool compatibility)
    if command -v ${picotool_DIR}/picotool >/dev/null 2>&1; then
        echo ""
        echo "Binary Info (picotool):"
        echo "-----------------------"
        ${picotool_DIR}/picotool info -a ${APP_DIR}/build/rp2040-converter.elf 2>&1 | grep -E "^(name|version|features|binary|program)" | head -10 || echo "  (Binary info extraction not available)"
    fi
    
    # Build configuration summary
    echo ""
    echo "Build Configuration:"
    echo "--------------------"
    echo "Board: ${PICO_BOARD}"
    echo "Platform: ${PICO_PLATFORM}"
    echo "Keyboard: ${KEYBOARD:-not specified}"
    echo "Mouse: ${MOUSE:-disabled}"
    echo "SDK Path: ${PICO_SDK_PATH}"
    
    echo ""
    echo "========================================"
    echo "Build Complete!"
    echo "========================================"
else
    echo "ERROR: Build failed - rp2040-converter.elf not found"
    exit 1
fi
