# Common compile options for all targets

target_link_libraries(${PROJECT_NAME} PUBLIC
  hardware_dma
  hardware_flash
  hardware_pio
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

# Enable runtime RAM execution check in debug builds
# This verifies that code is executing from SRAM (0x20000000-0x20042000)
# rather than Flash (0x10000000-0x15FFFFFF), which is critical for
# timing-sensitive PIO operations.
if(CMAKE_BUILD_TYPE MATCHES Debug)
  # Use add_compile_definitions for safety - works even if target not yet defined
  # Will be applied to ${PROJECT_NAME} target when it's created
  add_compile_definitions(RUN_FROM_RAM_CHECK)
  message(STATUS "Debug build: Enabling runtime RAM execution check")
endif()
