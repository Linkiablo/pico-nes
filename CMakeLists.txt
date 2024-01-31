cmake_minimum_required(VERSION 3.13)

set(FAMILY rp2040)
set(BOARD pico_sdk)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(my_project)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(pico_nes
        pico_nes.c
        usb_descriptors.c
)

target_include_directories(pico_nes PUBLIC
        ${CMAKE_CURRENT_LIST_DIR})

# pull in common dependencies and additional i2c hardware support
target_link_libraries(pico_nes PUBLIC pico_stdlib hardware_i2c pico_unique_id tinyusb_device tinyusb_board)

# enable usb output, disable uart output
pico_enable_stdio_usb(pico_nes 1)
pico_enable_stdio_uart(pico_nes 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(pico_nes)
