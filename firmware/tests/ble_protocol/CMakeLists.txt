# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble_protocol LANGUAGES C CXX)
set(app_root ${CMAKE_CURRENT_SOURCE_DIR}/../../common)
target_sources(app PRIVATE src/main.cpp
    ${app_root}/src/services/ble_protocol.cpp
    ${app_root}/src/services/qr.cpp
    ${app_root}/src/services/test_pattern.cpp)
target_include_directories(app PRIVATE ${app_root}/include)
target_compile_options(app PRIVATE -Wextra -Werror)
option(BADGE_SIMULATE_TARGET_ERRNO "Exercise the DK libc EINPROGRESS value" OFF)
if(BADGE_SIMULATE_TARGET_ERRNO)
    target_compile_options(app PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/target_errno.h)
endif()
add_subdirectory(${app_root}/lib/qrcodegen qrcodegen)
target_link_libraries(app PRIVATE badge_qrcodegen)
