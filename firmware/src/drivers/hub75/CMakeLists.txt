# SPDX-License-Identifier: MIT

zephyr_library()
zephyr_library_sources(hub75.c)
zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../include)
zephyr_library_compile_options(-Wextra -Werror)
