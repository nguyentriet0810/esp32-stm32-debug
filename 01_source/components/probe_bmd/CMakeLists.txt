# SPDX-License-Identifier: GPL-3.0-or-later
get_filename_component(bmd_dir "${CMAKE_CURRENT_LIST_DIR}/../../external/blackmagic" ABSOLUTE)
set(bmd_src "${bmd_dir}/src")
set(core command.c crc32.c exception.c gdb_main.c gdb_packet.c hex_utils.c maths_utils.c
    morse.c remote.c timing.c)
set(targets adi.c adiv5.c adiv5_jtag.c adiv5_swd.c adiv6.c gdb_reg.c jtag_devs.c
    jtag_scan.c onboard_flash.c semihosting.c sfdp.c spi.c target.c target_flash.c target_probe.c
    cortex.c cortexm.c stm32_common.c stm32f1.c stm32f4.c stm32g0.c stm32h5.c stm32h7.c
    stm32h7rs.c stm32l0.c stm32l4.c stm32mp15.c stm32wb0.c)
list(TRANSFORM core PREPEND "${bmd_src}/")
list(TRANSFORM targets PREPEND "${bmd_src}/target/")
set(VCS_TAG "v2.1.0-449730f8")
configure_file("${bmd_src}/include/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h" @ONLY)
idf_component_register(
    SRCS ${core} ${targets} "port/libc_compat.c"
    INCLUDE_DIRS "port" "${bmd_src}/include" "${bmd_src}" "${bmd_src}/target"
    PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/generated"
    WHOLE_ARCHIVE)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    CONFIG_POINTER_SIZE=4 NO_LIBOPENCM3=1 ADVERTISE_NOACKMODE=1)
target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_CORTEXM=1 CONFIG_STM=1)
target_compile_options(${COMPONENT_LIB} PUBLIC
    -include "${CMAKE_CURRENT_LIST_DIR}/port/libc_compat.h")
