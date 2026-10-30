# SPDX-License-Identifier: GPL-3.0-or-later
idf_component_register(SRCS "probe_console.c" INCLUDE_DIRS "include" REQUIRES esp_common PRIV_REQUIRES probe_network probe_debug probe_state heap freertos log)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
 include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/project_options.cmake")
 probe_apply_c_options(${COMPONENT_LIB} SDK_ADAPTER)
endif()
