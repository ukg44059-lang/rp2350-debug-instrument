set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/../boards)
set(PICO_BOARD waveshare_rp2350b_plus_w)

cmake_minimum_required(VERSION 3.13)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picow_wifi_scan C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(picow_wifi_scan_background
        picow_wifi_scan.c
)

target_include_directories(picow_wifi_scan_background PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
)

target_link_libraries(picow_wifi_scan_background
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
        hardware_pwm
)

pico_enable_stdio_usb(picow_wifi_scan_background 1)

pico_add_extra_outputs(picow_wifi_scan_background)

add_executable(picow_wifi_scan_poll
        picow_wifi_scan.c
)

target_include_directories(picow_wifi_scan_poll PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
)

target_link_libraries(picow_wifi_scan_poll
        pico_cyw43_arch_lwip_poll
        pico_stdlib
        hardware_pwm
        hardware_pio
)

pico_enable_stdio_usb(picow_wifi_scan_poll 1)
pico_enable_stdio_uart(picow_wifi_scan_poll 1)

pico_add_extra_outputs(picow_wifi_scan_poll)

