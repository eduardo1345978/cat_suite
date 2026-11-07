# Cat Box Pocket firmware for the Pimoroni Pico Plus 2.
#   cmake -B build -S pico -DPICO_SDK_PATH=/path/to/pico-sdk && cmake --build build   ->  build/cat_box_pocket.uf2
cmake_minimum_required (VERSION 3.13)
set (PICO_BOARD pimoroni_pico_plus2_rp2350 CACHE STRING "")
set (PICO_PLATFORM rp2350-arm-s CACHE STRING "")
include ($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project (cat_box_pocket C CXX ASM)
set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 17)
pico_sdk_init ()

add_executable (cat_box_pocket
    main.cpp usb_descriptors.c
    ../engine/engine.cpp ../engine/ui.cpp ../engine/store.cpp)
pico_generate_pio_header (cat_box_pocket ${CMAKE_CURRENT_LIST_DIR}/i2s.pio)
pico_generate_pio_header (cat_box_pocket ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
target_include_directories (cat_box_pocket PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_options (cat_box_pocket PRIVATE -O2 -Wall -Wno-misleading-indentation)
target_link_libraries (cat_box_pocket PRIVATE
    pico_stdlib pico_multicore pico_flash hardware_pio hardware_dma hardware_i2c hardware_spi hardware_uart hardware_adc hardware_flash
    tinyusb_device tinyusb_board)
pico_set_program_name (cat_box_pocket "Cat Box Pocket")
pico_enable_stdio_usb (cat_box_pocket 0)
pico_enable_stdio_uart (cat_box_pocket 0)
pico_add_extra_outputs (cat_box_pocket)
