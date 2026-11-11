
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/../boards)
set(PICO_BOARD waveshare_rp2350b_plus_w)

cmake_minimum_required(VERSION 3.13)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picow_blink_slow_clock C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(picow_blink
        picow_blink.c
        )
target_link_libraries(picow_blink
        pico_stdlib              # for core functionality
        pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
        )

# create map/bin/hex file etc.
pico_enable_stdio_uart(picow_blink 1)
pico_enable_stdio_usb(picow_blink 1)
pico_add_extra_outputs(picow_blink)

# add url via pico_set_program_url

# This version should behave exactly the same, but it runs the sys clock slower and changes the pio pio clock divisor for the cyw43 driver at run time.
add_executable(picow_blink_slow_clock
        picow_blink_slow_clock.c
        )
target_link_libraries(picow_blink_slow_clock
        pico_stdlib              # for core functionality
        pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
        hardware_clocks
        )
# This requires us to modify the pio divisor to successfully communicate with the cyw43 chip
target_compile_definitions(picow_blink_slow_clock PRIVATE
        CYW43_PIO_CLOCK_DIV_DYNAMIC=1
        )
pico_enable_stdio_uart(picow_blink_slow_clock 1)
pico_enable_stdio_usb(picow_blink_slow_clock 1)
# create map/bin/hex file etc.
pico_add_extra_outputs(picow_blink_slow_clock)

# add url via pico_set_program_url

# This version should behave exactly the same, but it runs the sys clock faster and changes the pio pio clock divisor for the cyw43 driver at build time.
add_executable(picow_blink_fast_clock
        picow_blink_fast_clock.c
        )
target_link_libraries(picow_blink_fast_clock
        pico_stdlib              # for core functionality
        pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
        hardware_clocks
        )
# This requires us to modify the pio divisor to successfully communicate with the cyw43 chip
target_compile_definitions(picow_blink_fast_clock PRIVATE
        CYW43_PIO_CLOCK_DIV_INT=4
        CYW43_PIO_CLOCK_DIV_FRAC8=0
        )
pico_enable_stdio_uart(picow_blink_fast_clock 1)
pico_enable_stdio_usb(picow_blink_fast_clock 1)
# create map/bin/hex file etc.
pico_add_extra_outputs(picow_blink_fast_clock)

# add url via pico_set_program_url
