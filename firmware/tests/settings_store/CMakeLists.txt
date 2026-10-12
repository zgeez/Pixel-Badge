# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(settings_store LANGUAGES C CXX)
set(app_root ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_sources(app PRIVATE src/main.cpp src/read_failures.cpp
	${app_root}/src/services/settings.cpp
	${app_root}/src/services/qr.cpp
	${app_root}/src/services/test_pattern.cpp
	${app_root}/src/platform/settings_store.cpp
)
target_include_directories(app PRIVATE ${app_root}/include)
target_compile_options(app PRIVATE -Wextra -Werror)
target_compile_definitions(app PRIVATE CONFIG_APP_LOG_LEVEL=3)
zephyr_ld_options(-Wl,--wrap=settings_load_subtree_direct)
add_subdirectory(${app_root}/lib/qrcodegen qrcodegen)
target_link_libraries(app PRIVATE badge_qrcodegen)
