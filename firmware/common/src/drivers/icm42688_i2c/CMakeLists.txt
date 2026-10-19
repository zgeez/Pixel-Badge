# SPDX-License-Identifier: MIT

zephyr_library_named(badge_icm42688)
zephyr_library_sources(icm42688_i2c.c)

# Register map shared with Zephyr's (SPI-only) ICM4268x driver.
zephyr_library_include_directories(${ZEPHYR_BASE}/drivers/sensor/tdk/icm4268x)

zephyr_library_compile_options(-Wextra -Werror)
