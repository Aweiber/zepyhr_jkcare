# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Let Zephyr discover the custom board under this application's boards/ tree.
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(jkcare)

zephyr_include_directories(include)

# Default SSRAM0 section and the dedicated Apollo4 Plus physical RAM banks.
zephyr_linker_sources(
    RAM_SECTIONS
    linker/jkcare-normal-ram.ld
)
zephyr_linker_sources(
    SECTIONS
    linker/jkcare-memory-regions.ld
)

target_sources(app PRIVATE
    src/main.c
    src/plugin_manager.c
)
