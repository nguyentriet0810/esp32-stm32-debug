# SPDX-License-Identifier: GPL-3.0-or-later

# @file CMakeLists.txt
# @brief probe_local_ui component registration and dependency visibility.
#
# Public dependencies support the exported headers; private dependencies remain implementation
# details.

idf_component_register(SRCS "probe_local_ui.c" "probe_ui_settings.c" INCLUDE_DIRS "include" REQUIRES esp_common PRIV_REQUIRES probe_oled probe_encoder probe_state freertos log esp_timer)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
 include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/project_options.cmake")
 probe_apply_c_options(${COMPONENT_LIB} SDK_ADAPTER)
endif()
