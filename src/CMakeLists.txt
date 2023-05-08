cmake_minimum_required(VERSION 3.18)

# initialize pico-sdk
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# define project
project(ibm-5170-pcat C CXX ASM)

# initialize the Raspberry Pi Pico SDK
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
${CMAKE_CURRENT_LIST_DIR}/keymap.c
${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
${CMAKE_CURRENT_LIST_DIR}/hid_interface.c
${CMAKE_CURRENT_LIST_DIR}/hid_keymaps.c
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
