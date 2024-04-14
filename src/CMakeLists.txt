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

if(NOT KEYBOARD)
  message(FATAL_ERROR
  "When building, you need to ensure you set the required Keyboard that will\n"
  "be used for the build.  You do this by passing the required Keyboard to\n"
  "the build command in docker-compose, for example:\n"
  "  docker-compose run -e KEYBOARD=\"modelf/6450225\" builder")
endif()

set(REQUIRED_KEYBOARD_FILES
  "${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/keyboard.h"
  "${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/keyboard.c"
  "${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/keyboard.config"
)

foreach(file IN LISTS REQUIRED_KEYBOARD_FILES)
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "File '${file}' does not exist!")
  endif()
endforeach()

# Read in Keyboard Config to ensure we include the correct build-time files
file(READ "${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/keyboard.config" KEYBOARD_CONFIG)

if(NOT KEYBOARD_CONFIG)
  message(WARNING "WARNING: Keyboard config file '${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/keyboard.config' is empty or doesn't exist.")
endif()

# Now parse KEYBOARD_CONFIG into a list of key/val pairs and set them.
# We only want to MATCHALL against actual key/val pairs, not comments or empty lines.
string(REGEX MATCHALL "[^\n\r][A-Z]+=[a-zA-Z0-9_/()-. ]+" KEYBOARD_CONFIG "${KEYBOARD_CONFIG}")

foreach(line ${KEYBOARD_CONFIG})
  # Find the index of the '=' character in the line
  string(FIND "${line}" "=" EQL_IDX)

  # Extract the part before '=' (left side of the equation) as the key name
  string(SUBSTRING "${line}" 0 ${EQL_IDX} KEY_NAME)

  # Calculate the index of the character after '='
  math(EXPR VAL_IDX "${EQL_IDX} + 1")

  # Extract the part after '=' as the key value
  string(SUBSTRING "${line}" ${VAL_IDX} -1 KEY_VAL)

  # Set the variable with the extracted key name to the extracted key value
  set(${KEY_NAME} "${KEY_VAL}")
endforeach()

set(REQUIRED_VARIABLES MAKE DESCRIPTION MODEL PROTOCOL CODESET)

set(MISSING_CONFIG_OPTIONS "")
foreach(variable ${REQUIRED_VARIABLES})
  if (NOT DEFINED ${variable})
    list(APPEND MISSING_CONFIG_OPTIONS "'${variable}'")
  endif()
endforeach()

if (MISSING_CONFIG_OPTIONS)
  message(FATAL_ERROR "Keyboard config file '${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/keyboard.config' is missing the following key(s): ${MISSING_CONFIG_OPTIONS}")
endif()

# Pass the required variables to the compiler
add_definitions(-D_KEYBOARD_MAKE="${MAKE}")
add_definitions(-D_KEYBOARD_DESCRIPTION="${DESCRIPTION}")
add_definitions(-D_KEYBOARD_MODEL="${MODEL}")
add_definitions(-D_KEYBOARD_PROTOCOL="${PROTOCOL}")
add_definitions(-D_KEYBOARD_CODESET="${CODESET}")

set(REQUIRED_PROTOCOL_FILES
  "${CMAKE_SOURCE_DIR}/protocols/${PROTOCOL}/interface.h"
  "${CMAKE_SOURCE_DIR}/protocols/${PROTOCOL}/interface.c"
)

set(REQUIRED_SCANCODE_FILES
  "${CMAKE_SOURCE_DIR}/scancodes/${CODESET}/scancode.h"
  "${CMAKE_SOURCE_DIR}/scancodes/${CODESET}/scancode.h"
)

foreach(file IN LISTS REQUIRED_PROTOCOL_FILES)
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "File '${file}' does not exist!")
  endif()
endforeach()

foreach(file IN LISTS REQUIRED_SCANCODE_FILES)
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "File '${file}' does not exist!")
  endif()
endforeach()

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set the project name
project(rp2040-kbd-converter C CXX ASM)

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

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_SOURCE_DIR}/
  ${CMAKE_SOURCE_DIR}/common/lib/
  ${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/
  ${CMAKE_SOURCE_DIR}/protocols/${PROTOCOL}/
  ${CMAKE_SOURCE_DIR}/scancodes/${CODESET}/
)

# Define list of common source files within project root
file(GLOB SRC_ROOT CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/*.c
)

# Define list of source files for common libraries
file(GLOB_RECURSE SRC_COMMON CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/common/*.c
)

file(GLOB SRC_KEYBOARD CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/*.c
)

file(GLOB SRC_PROTOCOL CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/protocols/${PROTOCOL}/*.c
)

file(GLOB SRC_SCANCODE CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/scancodes/${CODESET}/*.c
)

# Define list of State Machine PIO files for compilation
file(GLOB PROTOCOL_PIO_FILES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/protocols/${PROTOCOL}/*.pio
)

# Define list of Common PIO files for compilation
file(GLOB_RECURSE COMMON_PIO_FILES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/common/*.pio
)

# Loop through each PIO file and generate header.
foreach(PIO_FILE IN LISTS COMMON_PIO_FILES PROTOCOL_PIO_FILES)
  # Extract directory and filename without extension
  get_filename_component(PIO_DIR ${PIO_FILE} DIRECTORY)
  get_filename_component(PIO_NAME ${PIO_FILE} NAME_WE)

  # Generate header filename and path
  set(PIO_HEADER ${PIO_DIR}/${PIO_NAME}.h)

  # Call pico_generate_pio_header with individual file and output path
  pico_generate_pio_header(${PROJECT_NAME}
    ${PIO_FILE}
  )
endforeach()

# Build all globs
target_sources(${PROJECT_NAME} PUBLIC
  ${SRC_ROOT}
  ${SRC_COMMON}
  ${SRC_KEYBOARD}
  ${SRC_PROTOCOL}
  ${SRC_SCANCODE}
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  hardware_pio
  hardware_pwm
  pico_stdlib
  pico_unique_id
  tinyusb_board
  tinyusb_device
)

target_compile_options(${PROJECT_NAME} PUBLIC
  # The following options blow up with errors from the tiny-usb library inclusion.
  # -pedantic
  # -Wconversion
  # Disable Shadow warnings which are caused by hardware_flash library.
  -Wno-shadow
  # The following options should be enabled.
  -Wformat=2
  -Wcast-qual
  -Wall
  -Werror
  -Wextra
  -Wunused
  -O2
)
