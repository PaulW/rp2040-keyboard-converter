#!/bin/bash

RP2040_MOUNTPOINT="/Volumes/RPI-RP2"
LOOP_CHECK=30
BUILD_FOLDER="build"
FW_EXTENSION="uf2"

DEVICE_NAME="Keyboard/Mouse (or other RP2040-based Device)"
RP2040_MOUNTED=false

echo "You are about to attempt to update the FIRMWARE on your connected ${DEVICE_NAME}!"
echo "By default, we will wait for '${RP2040_MOUNTPOINT}' to be mounted."
echo "To enter BootLoader mode, on the Keyboard, press the following combination:"
echo ""
echo "[Fn] + [L Shift] + [R Shift] + [B]"
echo ""
echo "If no Keyboard is attached, power off the device, hold down the BOOTSEL button,"
echo "power on the device, and then release the BOOTSEL button."
echo ""
echo "Press Ctrl+C to exit this..."
echo -ne "Waiting for ${DEVICE_NAME} to enter bootloader mode."

for ((i = 0; i < ${LOOP_CHECK}; i++)); do
    if mount | grep -q " ${RP2040_MOUNTPOINT} "; then
        echo "*"
        echo "${DEVICE_NAME} is mounted! Waiting 5 seconds to stabalise..."
        sleep 5
        RP2040_MOUNTED=true
        break
    else
        sleep 1
        echo -ne "."
    fi
done

if [ "${RP2040_MOUNTED}" == false ]; then
    echo "X"
    echo "${DEVICE_NAME} was not detected.  Is one connected & in Bootloader Mode?"
    exit 1
fi

LATEST_FIRMWARE=$(find "${BUILD_FOLDER}" -type f -name "*${FW_EXTENSION}" -exec ls -t {} + | head -n 1)

if [ -n "${LATEST_FIRMWARE}" ]; then
    echo "Found Firmware file '${LATEST_FIRMWARE}'!"
else
    echo "Error: No Firmware files found within '${BUILD_FOLDER}'.  Did it build OK?"
    exit 1
fi

echo "Attempting to Flash ${DEVICE_NAME}..."
echo "Once flashed, the attached device will automatically unmount (you may get a warning about this) and then attempt to boot your new Firmware!"
echo "If the device fails to boot (or you have any other issues) you will need to revert or resolve the issue and enter bootloader mode manually."

cp -v "${LATEST_FIRMWARE}" "${RP2040_MOUNTPOINT}/"

echo "Done!"
