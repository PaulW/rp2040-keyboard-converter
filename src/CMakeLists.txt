# This file is part of Model-F 5170 Converter.
#
# Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
#
# Model-F 5170 Converter is free software: you can redistribute it
# and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# Model-F 5170 Converter is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Model-F 5170 Converter.
# If not, see <https://www.gnu.org/licenses/>.

# Default Build Options
# These set default options for when we build/compile so to avoide having to modify source files directly.

set(KEYBOARD_LAYOUT "6450225") # Defines the Keyboard Layout which we will be using (see keymaps folder for options)

# End of Default Build Options

# Do not modify below this line unless you know what you are doing.
cmake_minimum_required(VERSION 3.18)

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set the project name
project(ibm-5170-pcat C CXX ASM)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(${PROJECT_NAME})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ../build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../build)

set_target_properties(${PROJECT_NAME}
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY ../build
  RUNTIME_OUTPUT_DIRECTORY ../build
)

pico_add_uf2_output(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} PUBLIC
${CMAKE_CURRENT_LIST_DIR}/lock_leds.c
${CMAKE_CURRENT_LIST_DIR}/pcat_interface.c
${CMAKE_CURRENT_LIST_DIR}/pcat_interface.pio
${CMAKE_CURRENT_LIST_DIR}/keymaps.c
${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
${CMAKE_CURRENT_LIST_DIR}/hid_interface.c
${CMAKE_CURRENT_LIST_DIR}/keymaps/${KEYBOARD_LAYOUT}/keymap.c
${CMAKE_CURRENT_LIST_DIR}/main.c
)

pico_generate_pio_header(${PROJECT_NAME}
  ${CMAKE_CURRENT_LIST_DIR}/pcat_interface.pio
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  hardware_pio
  pico_stdlib
  tinyusb_device
  tinyusb_board
)

target_compile_options(${PROJECT_NAME} PUBLIC
  # The following options blow up with errors from the tiny-usb library inclusion.
  # -pedantic
  # -Wconversion
  # The following options should be enabled.
  -Wshadow
  -Wformat=2
  -Wcast-qual
  -Wall
  -Werror
  -Wextra
  -Wunused
  -O2
)
