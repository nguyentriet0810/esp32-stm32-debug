# SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
# SPDX-License-Identifier: GPL-3.0-or-later

include_guard(GLOBAL)

option(PROBE_ENABLE_GCC_ANALYZER "Run GCC static analysis on project-owned C targets" ON)

function(probe_apply_c_options target_name)
    set_target_properties(${target_name} PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )

    # IDF adds -std=gnu23 through a transitive interface after target C_STANDARD.
    # Source options come last, so the project-owned translation units use C17.
    get_target_property(probe_sources ${target_name} SOURCES)
    foreach(probe_source IN LISTS probe_sources)
        if(probe_source MATCHES "\\.c$")
            set_property(SOURCE "${probe_source}" APPEND PROPERTY COMPILE_OPTIONS -std=c17)
        endif()
    endforeach()

    set(probe_c_options
        -Wall
        -Wextra
        -Werror
        -Wformat=2
        -Wformat-signedness
        -Wshadow
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wconversion
        -Wsign-conversion
    )
    if(PROBE_ENABLE_GCC_ANALYZER)
        list(APPEND probe_c_options -fanalyzer)
    endif()
    foreach(probe_c_option IN LISTS probe_c_options)
        target_compile_options(${target_name} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:${probe_c_option}>"
        )
    endforeach()
endfunction()
