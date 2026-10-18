# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(display_health LANGUAGES C CXX)
set(app_root ${CMAKE_CURRENT_SOURCE_DIR}/../../common)
target_sources(app PRIVATE src/main.cpp
	${app_root}/src/services/display_health.cpp
	${app_root}/src/services/boot_guard.cpp
)
target_include_directories(app PRIVATE ${app_root}/include)
target_compile_options(app PRIVATE -Wextra -Werror)
