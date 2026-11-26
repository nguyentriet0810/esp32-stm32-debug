# SPDX-License-Identifier: GPL-3.0-or-later
# @file CMakeLists.txt
# @brief AP web portal, bounded form codec and router ping integration.
idf_component_register(SRCS "probe_wifi_setup.c" "probe_wifi_form.c"
    INCLUDE_DIRS "include" EMBED_TXTFILES "wifi_portal.html"
    REQUIRES esp_common
    PRIV_REQUIRES esp_http_server esp_netif esp_wifi esp_hw_support lwip freertos
                  probe_network probe_debug probe_state log)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/project_options.cmake")
    probe_apply_c_options(${COMPONENT_LIB} SDK_ADAPTER)
endif()
