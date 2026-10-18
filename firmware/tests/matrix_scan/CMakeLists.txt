# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(matrix_scan C)
target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE ../../common/include)
target_compile_options(app PRIVATE -Wextra -Werror)
