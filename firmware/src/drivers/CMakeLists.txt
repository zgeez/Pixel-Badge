# SPDX-License-Identifier: MIT

zephyr_library()
zephyr_library_sources(bus_lock.c)
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory_ifdef(CONFIG_ICM42688_I2C icm42688_i2c)
add_subdirectory_ifdef(CONFIG_HUB75_DISPLAY hub75)
