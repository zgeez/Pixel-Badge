# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sanity LANGUAGES C CXX)

target_sources(app PRIVATE
	src/main.cpp
)

target_compile_options(app PRIVATE -Wextra -Werror)
