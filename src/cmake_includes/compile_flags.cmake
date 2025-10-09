# Common compile options for all targets

target_link_libraries(${PROJECT_NAME} PUBLIC
  hardware_dma
  hardware_flash
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