# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smart_led_badge LANGUAGES C CXX)

target_sources(app PRIVATE
	src/main.cpp
)

target_include_directories(app PRIVATE include)

# Strict warnings for application-owned sources only. These options apply to
# the `app` target and do not change how Zephyr or third-party modules build.
target_compile_options(app PRIVATE -Wextra -Werror)
