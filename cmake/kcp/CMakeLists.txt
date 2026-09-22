cmake_minimum_required(VERSION 3.25)

project(pans-kcp LANGUAGES C)

include(GNUInstallDirs)

if(NOT PANS_KCP_SOURCE_DIR)
    message(FATAL_ERROR "PANS_KCP_SOURCE_DIR is required")
endif()
if(NOT EXISTS "${PANS_KCP_SOURCE_DIR}/ikcp.c" OR
   NOT EXISTS "${PANS_KCP_SOURCE_DIR}/ikcp.h")
    message(FATAL_ERROR "Invalid KCP source directory: ${PANS_KCP_SOURCE_DIR}")
endif()

add_library(kcp STATIC "${PANS_KCP_SOURCE_DIR}/ikcp.c")
add_library(kcp::kcp ALIAS kcp)

target_include_directories(kcp PUBLIC
    $<BUILD_INTERFACE:${PANS_KCP_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(FILES "${PANS_KCP_SOURCE_DIR}/ikcp.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(TARGETS kcp EXPORT kcp-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(EXPORT kcp-targets
    FILE kcp-config.cmake
    NAMESPACE kcp::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/kcp"
)
