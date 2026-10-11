# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20.0)

# Project drivers (Kconfig, devicetree bindings, sources) as a Zephyr module.
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smart_led_badge LANGUAGES C CXX)

target_sources(app PRIVATE
	src/main.cpp
	src/app/bringup.cpp
	src/services/test_pattern.cpp
)
target_sources_ifdef(CONFIG_DISPLAY app PRIVATE src/app/display_demo.cpp)

target_include_directories(app PRIVATE include)

# Strict warnings for application-owned sources only. These options apply to
# the `app` target and do not change how Zephyr or third-party modules build.
target_compile_options(app PRIVATE -Wextra -Werror)
