# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Default to Debug so the ELF has -Og -g for step-through debugging via the debug probe.
# Override with: cmake -DCMAKE_BUILD_TYPE=Release ..
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(TFT_display_touch_driver C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable for testbench
add_executable(testbench 
    main_testbench.c
    spi_interface.c
    ili9341_display.c
    xpt2046_touch.c
)

pico_set_program_name(testbench "Display Touch Testbench")
pico_set_program_version(testbench "1.0")

# no_flash means the target is to run from RAM
pico_set_binary_type(testbench no_flash)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(testbench 1)
pico_enable_stdio_usb(testbench 0)

target_link_libraries(testbench
        pico_stdlib
        hardware_spi
        hardware_dma
)

target_include_directories(testbench PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(testbench)

