# CMAKE script for building the keyboard firmware
# This script is responsible for setting up the build environment
# and ensuring that all required files are present before building.

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

# Add prefix to key name
set(KEY_NAME "KEYBOARD_${KEY_NAME}")

# Calculate the index of the character after '='
math(EXPR VAL_IDX "${EQL_IDX} + 1")

# Extract the part after '=' as the key value
string(SUBSTRING "${line}" ${VAL_IDX} -1 KEY_VAL)

# Set the variable with the extracted key name to the extracted key value
set(${KEY_NAME} "${KEY_VAL}")
endforeach()

set(REQUIRED_VARIABLES KEYBOARD_MAKE KEYBOARD_DESCRIPTION KEYBOARD_MODEL KEYBOARD_PROTOCOL KEYBOARD_CODESET)

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
add_definitions(-D_KEYBOARD_ENABLED=1)
add_definitions(-D_KEYBOARD_MAKE="${KEYBOARD_MAKE}")
add_definitions(-D_KEYBOARD_DESCRIPTION="${KEYBOARD_DESCRIPTION}")
add_definitions(-D_KEYBOARD_MODEL="${KEYBOARD_MODEL}")
add_definitions(-D_KEYBOARD_PROTOCOL="${KEYBOARD_PROTOCOL}")
add_definitions(-D_KEYBOARD_CODESET="${KEYBOARD_CODESET}")

set(REQUIRED_KEYBOARD_FILES
  "${CMAKE_SOURCE_DIR}/protocols/${KEYBOARD_PROTOCOL}/keyboard_interface.h"
  "${CMAKE_SOURCE_DIR}/protocols/${KEYBOARD_PROTOCOL}/keyboard_interface.c"
)

set(REQUIRED_KEYBOARD_SCANCODE_FILES
  "${CMAKE_SOURCE_DIR}/scancodes/${KEYBOARD_CODESET}/scancode.h"
  "${CMAKE_SOURCE_DIR}/scancodes/${KEYBOARD_CODESET}/scancode.h"
)

foreach(file IN LISTS REQUIRED_KEYBOARD_FILES)
if(NOT EXISTS "${file}")
  message(FATAL_ERROR "File '${file}' does not exist!")
endif()
endforeach()

foreach(file IN LISTS REQUIRED_KEYBOARD_SCANCODE_FILES)
if(NOT EXISTS "${file}")
  message(FATAL_ERROR "File '${file}' does not exist!")
endif()
endforeach()

file(GLOB SRC_KEYBOARD CONFIGURE_DEPENDS
${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/*.c
)

file(GLOB SRC_KEYBOARD_PROTOCOL CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/protocols/${KEYBOARD_PROTOCOL}/common_interface.c
  ${CMAKE_SOURCE_DIR}/protocols/${KEYBOARD_PROTOCOL}/keyboard_*.c
)

# Link scancode processor files
file(GLOB SRC_KEYBOARD_SCANCODE CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/scancodes/${KEYBOARD_CODESET}/*.c
)

# Define list of State Machine PIO files for compilation.
file(GLOB KEYBOARD_PROTOCOL_PIO_FILES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/protocols/${KEYBOARD_PROTOCOL}/keyboard_*.pio
  ${CMAKE_SOURCE_DIR}/protocols/${KEYBOARD_PROTOCOL}/interface.pio
)
list(APPEND PIO_FILES ${KEYBOARD_PROTOCOL_PIO_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_SOURCE_DIR}/keyboards/${KEYBOARD}/
  ${CMAKE_SOURCE_DIR}/protocols/${KEYBOARD_PROTOCOL}/
  ${CMAKE_SOURCE_DIR}/scancodes/${KEYBOARD_CODESET}/
)

target_sources(${PROJECT_NAME} PUBLIC
  ${SRC_KEYBOARD}
  ${SRC_KEYBOARD_PROTOCOL}
  ${SRC_KEYBOARD_SCANCODE}
)