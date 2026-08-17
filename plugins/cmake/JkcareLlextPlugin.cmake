# SPDX-License-Identifier: Apache-2.0

function(jkcare_add_llext_plugin plugin_name plugin_source)
    if(NOT DEFINED LLEXT_EDK_INSTALL_DIR)
        set(LLEXT_EDK_INSTALL_DIR "$ENV{LLEXT_EDK_INSTALL_DIR}")
    endif()

    if(NOT EXISTS "${LLEXT_EDK_INSTALL_DIR}/cmake.cflags")
        message(FATAL_ERROR
            "LLEXT_EDK_INSTALL_DIR does not contain cmake.cflags: "
            "${LLEXT_EDK_INSTALL_DIR}"
        )
    endif()

    include("${LLEXT_EDK_INSTALL_DIR}/cmake.cflags")

    if(NOT DEFINED JKCARE_PLUGIN_API_DIR)
        get_filename_component(JKCARE_PLUGIN_API_DIR
            "${CMAKE_CURRENT_LIST_DIR}/../../include" ABSOLUTE
        )
    endif()

    add_compile_options(${LLEXT_CFLAGS} -c)
    get_property(plugin_compile_options DIRECTORY PROPERTY COMPILE_OPTIONS)

    set(plugin_output "${PROJECT_BINARY_DIR}/${plugin_name}.llext")

    add_custom_command(
        OUTPUT "${plugin_output}"
        COMMAND "${CMAKE_C_COMPILER}"
            ${plugin_compile_options}
            "-I${JKCARE_PLUGIN_API_DIR}"
            -o "${plugin_output}"
            "${plugin_source}"
        DEPENDS
            "${plugin_source}"
            "${JKCARE_PLUGIN_API_DIR}/jkcare_plugin.h"
        VERBATIM
    )

    add_custom_target(${plugin_name} ALL DEPENDS "${plugin_output}")
endfunction()
