# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20.0)
get_filename_component(BADGE_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(BADGE_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common" ABSOLUTE)
list(APPEND ZEPHYR_EXTRA_MODULES ${BADGE_COMMON_DIR}/src/drivers)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf5340_prototype LANGUAGES C CXX)
include(${BADGE_COMMON_DIR}/application.cmake)
