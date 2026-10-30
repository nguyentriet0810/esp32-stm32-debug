# SPDX-FileCopyrightText: 2026 Nguyen Hoang Minh Triet
# SPDX-License-Identifier: GPL-3.0-or-later

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES probe_board probe_network probe_debug probe_console probe_local_ui nvs_flash log
)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    include("${CMAKE_CURRENT_LIST_DIR}/../cmake/project_options.cmake")
    probe_apply_c_options(${COMPONENT_LIB})
endif()
