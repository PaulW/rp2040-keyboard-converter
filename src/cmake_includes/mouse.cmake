# CMAKE script for building the mouse firmware
# This script is responsible for setting up the build environment
# and ensuring that all required files are present before building.

# Unlike Keyboard Firmnware, the mouse firmware is built solely from 
# the Protocol files.

add_definitions(-D_MOUSE_ENABLED=1)
add_definitions(-D_MOUSE_PROTOCOL="${MOUSE}")

set(REQUIRED_MOUSE_PROTOCOL_FILES
  "${CMAKE_SOURCE_DIR}/protocols/${MOUSE}/mouse_interface.h"
  "${CMAKE_SOURCE_DIR}/protocols/${MOUSE}/mouse_interface.c"
)

foreach(file IN LISTS REQUIRED_MOUSE_PROTOCOL_FILES)
  if(NOT EXISTS ${file})
    message(FATAL_ERROR "Required file ${file} not found.")
  endif()
endforeach()

# Define list of State Machine PIO files for compilation.
file(GLOB MOUSE_PROTOCOL_PIO_FILES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/protocols/${MOUSE}/mouse_*.pio
  ${CMAKE_SOURCE_DIR}/protocols/${MOUSE}/interface.pio
)
list(APPEND PIO_FILES ${MOUSE_PROTOCOL_PIO_FILES})

file(GLOB SRC_MOUSE_FILES CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/protocols/${MOUSE}/common_interface.c
  ${CMAKE_SOURCE_DIR}/protocols/${MOUSE}/mouse_*.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_SOURCE_DIR}/protocols/${MOUSE}/
)

target_sources(${PROJECT_NAME} PUBLIC
  ${SRC_MOUSE_FILES}
)