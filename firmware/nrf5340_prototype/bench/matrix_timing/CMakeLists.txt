# SPDX-License-Identifier: MIT
#
# Bench test F1: Rev A matrix scan timing on the nRF5340 DK. Not part of the
# badge image; built for the DK with the bench overlay.

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(matrix_timing LANGUAGES C)

target_sources(app PRIVATE src/main.c)
target_compile_options(app PRIVATE -Wall -Wextra -Werror)
