# SPDX-License-Identifier: MIT
zephyr_library_named(badge_reva_matrix)
zephyr_library_include_directories(../../../include)
zephyr_library_sources(reva_matrix.c)
zephyr_library_compile_options(-Wextra -Werror)
