cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(my_project)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(pico_nes
        pico_nes.c
        )

# pull in common dependencies and additional i2c hardware support
target_link_libraries(pico_nes pico_stdlib hardware_i2c)

# enable usb output, disable uart output
pico_enable_stdio_usb(pico_nes 1)
pico_enable_stdio_uart(pico_nes 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(pico_nes)
