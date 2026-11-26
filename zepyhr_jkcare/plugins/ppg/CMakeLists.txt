# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../cmake/toolchain.cmake"
)
set(CMAKE_C_COMPILER_FORCED TRUE)

project(ppg_plugin C)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/JkcareLlextPlugin.cmake")
jkcare_add_llext_plugin(ppg_plugin "${CMAKE_CURRENT_LIST_DIR}/ppg_plugin.c")
