# SPDX-License-Identifier: GPL-3.0-or-later

# @file CMakeLists.txt
# @brief probe_console component registration and dependency visibility.
#
# Public dependencies support the exported headers; private dependencies remain implementation
# details.

idf_component_register(SRCS "probe_console.c" INCLUDE_DIRS "include" REQUIRES esp_common PRIV_REQUIRES probe_network probe_debug probe_state probe_sd heap freertos log)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
 include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/project_options.cmake")
 probe_apply_c_options(${COMPONENT_LIB} SDK_ADAPTER)
endif()
