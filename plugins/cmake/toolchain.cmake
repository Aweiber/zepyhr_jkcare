# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED ENV{ZEPHYR_SDK_INSTALL_DIR})
    message(FATAL_ERROR "ZEPHYR_SDK_INSTALL_DIR is not set")
endif()

if(WIN32)
    set(TOOLCHAIN_SUFFIX ".exe")
else()
    set(TOOLCHAIN_SUFFIX "")
endif()

set(ZEPHYR_ARM_TOOLCHAIN
    "$ENV{ZEPHYR_SDK_INSTALL_DIR}/gnu/arm-zephyr-eabi"
)

set(CMAKE_C_COMPILER
    "${ZEPHYR_ARM_TOOLCHAIN}/bin/arm-zephyr-eabi-gcc${TOOLCHAIN_SUFFIX}"
)

set(CMAKE_FIND_ROOT_PATH
    "${ZEPHYR_ARM_TOOLCHAIN}/arm-zephyr-eabi"
)
