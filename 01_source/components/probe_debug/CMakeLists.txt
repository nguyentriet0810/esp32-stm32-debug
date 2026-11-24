# SPDX-License-Identifier: GPL-3.0-or-later

# @file CMakeLists.txt
# @brief probe_debug component registration and dependency visibility.
#
# Public dependencies support the exported headers; private dependencies remain implementation
# details.

idf_component_register(
    SRCS "probe_debug.c" "port/probe_swd.c" "port/probe_platform.c" "port/probe_jtag.c"
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "port" REQUIRES esp_common
    PRIV_REQUIRES probe_bmd probe_state esp_driver_gpio esp_hal_gpio esp_timer
        lwip freertos log esp_hw_support)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/project_options.cmake")
    probe_apply_c_options(${COMPONENT_LIB} SDK_ADAPTER)
endif()
