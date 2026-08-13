# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Let Zephyr discover the custom board under this application's boards/ tree.
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(jkcare)

zephyr_include_directories(include)

target_sources(app PRIVATE
    src/main.c
    src/plugin_manager.c
)

foreach(plugin_name ppg ecg)
    set(plugin_binary ${PROJECT_BINARY_DIR}/llext/${plugin_name}_plugin.llext)
    set(plugin_include ${ZEPHYR_BINARY_DIR}/include/generated/${plugin_name}_plugin.inc)

    add_llext_target(${plugin_name}_plugin
        OUTPUT ${plugin_binary}
        SOURCES plugins/${plugin_name}/${plugin_name}_plugin.c
    )

    generate_inc_file_for_target(app ${plugin_binary} ${plugin_include})
endforeach()

add_subdirectory(drivers/sensor/cht8305c)
