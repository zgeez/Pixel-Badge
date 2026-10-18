# SPDX-License-Identifier: MIT

zephyr_library_named(badge_bus)
zephyr_library_sources(bus_lock.c)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory_ifdef(CONFIG_ICM42688_I2C icm42688_i2c)
add_subdirectory_ifdef(CONFIG_HUB75_DISPLAY hub75)

add_subdirectory_ifdef(CONFIG_REVA_MATRIX reva_matrix)
