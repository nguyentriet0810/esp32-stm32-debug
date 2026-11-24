# SPDX-License-Identifier: GPL-3.0-or-later

# @file CMakeLists.txt
# @brief probe_sd component registration and dependency visibility.
#
# Public dependencies support the exported headers; private dependencies remain implementation
# details.

idf_component_register(SRCS "probe_sd.c" INCLUDE_DIRS "include" REQUIRES esp_common PRIV_REQUIRES fatfs sdmmc esp_driver_sdspi esp_driver_spi esp_driver_gpio freertos)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
 include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/project_options.cmake")
 probe_apply_c_options(${COMPONENT_LIB} SDK_ADAPTER)
endif()
