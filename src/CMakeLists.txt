# This file is part of RP2040 Keyboard Converter.
#
# Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
#
# RP2040 Keyboard Converter is free software: you can redistribute it
# and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# RP2040 Keyboard Converter is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with RP2040 Keyboard Converter.
# If not, see <https://www.gnu.org/licenses/>.

# Do not modify below this line unless you know what you are doing.
# This is set to the CMAKE Version in debian:stable-slim as of 29/02/2024
cmake_minimum_required(VERSION 3.25.1 FATAL_ERROR)

# Set the build time as a string so we can use it in the code
string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M")
add_definitions(-D_BUILD_TIME="${BUILD_TIME}")
message("Build Time: ${BUILD_TIME}")

# Ensure we set the required Keyboard which we want to build with
# Access the environment variable and ensure it's valid
if (DEFINED ENV{KEYBOARD})
  set(KEYBOARD $ENV{KEYBOARD})
endif()

if(DEFINED ENV{MOUSE})
  set(MOUSE $ENV{MOUSE})
endif()

if(NOT KEYBOARD AND NOT MOUSE)
  message(FATAL_ERROR
  "When building, you need to ensure you set the required Keyboard or Mouse "
  "that will be used for the build.  You do this by passing the required "
  "device to the build command in docker compose, for example:\n"
  "  docker compose run -e KEYBOARD=\"modelf/pcat\" builder\n"
  "or\n"
  "  docker compose run -e MOUSE=\"at-ps2\" builder\n"
  "You can build for either a Keyboard or Mouse, or both.\n"
  "Some Keyboards may have an inbuilt Mouse, so this will automatically be "
  "set when building for that Keyboard, anbd as such, any user-specified "
  "`MOUSE` option specified will be ignored in favour of the built-in device..\n")
endif()

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set the project name
project(rp2040-converter C CXX ASM)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(${PROJECT_NAME})

# Ensure program executes from RAM for faster performance
# This way we don't need to set any function or variable declarators.
pico_set_binary_type(${PROJECT_NAME} copy_to_ram)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ../build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../build)

set_target_properties(${PROJECT_NAME}
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY ../build
  RUNTIME_OUTPUT_DIRECTORY ../build
)

pico_add_uf2_output(${PROJECT_NAME})

SET(PIO_FILES "")

# Build all globs
target_sources(${PROJECT_NAME} PUBLIC
  ${SRC_ROOT}
  ${SRC_COMMON}
  ${SRC_KEYBOARD}
  ${SRC_PROTOCOL}
  ${SRC_SCANCODE}
)

include(${CMAKE_SOURCE_DIR}/cmake_includes/common.cmake)

# If we have a Keyboard, include the Keyboard cmake file
if(KEYBOARD)
  include(${CMAKE_SOURCE_DIR}/cmake_includes/keyboard.cmake)
endif()

# If we have a Keyboard with a built-in Mouse, set the Mouse to the one
# defined within the Keyboard Config. However, if the user has specified
# a Mouse, then use that instead, as we may not want to use the built-in Mouse.
if(KEYBOARD_MOUSE AND NOT MOUSE)
  set(MOUSE ${KEYBOARD_MOUSE})
endif()

# If we have a Mouse, include the Mouse cmake file
if(MOUSE)
  include(${CMAKE_SOURCE_DIR}/cmake_includes/mouse.cmake)
endif()

# Loop through each PIO file and generate header.
list(REMOVE_DUPLICATES PIO_FILES)
foreach(PIO_FILE IN LISTS PIO_FILES)
  # Extract directory and filename without extension
  get_filename_component(PIO_DIR ${PIO_FILE} DIRECTORY)
  get_filename_component(PIO_NAME ${PIO_FILE} NAME_WE)

  # Call pico_generate_pio_header with individual file and output path
  pico_generate_pio_header(${PROJECT_NAME}
    ${PIO_FILE}
  )
endforeach()

include(${CMAKE_SOURCE_DIR}/cmake_includes/compile_flags.cmake)