cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "Load ESP-IDF v6.1 first, or use 01_source/tools/idf.ps1.")
endif()

include("$ENV{IDF_PATH}/tools/cmake/version.cmake")
if(NOT IDF_VERSION_MAJOR EQUAL 6 OR NOT IDF_VERSION_MINOR EQUAL 1)
    message(FATAL_ERROR
        "This project requires ESP-IDF v6.1.x; found "
        "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}.")
endif()

set(IDF_TARGET "esp32s3" CACHE STRING "Probe hardware target")
if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "This hardware profile requires ESP32-S3.")
endif()

set(PROJECT_VER "0.1.0")
include("$ENV{IDF_PATH}/tools/cmake/project.cmake")
idf_build_set_property(MINIMAL_BUILD ON)
project(stm32_wifi_probe)
