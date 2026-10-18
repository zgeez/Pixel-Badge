# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(modes LANGUAGES C CXX)
set(app_root ${CMAKE_CURRENT_SOURCE_DIR}/../../common)
target_sources(app PRIVATE src/main.cpp
	${app_root}/src/services/text.cpp
	${app_root}/src/services/content.cpp
	${app_root}/src/services/qr.cpp
	${app_root}/src/services/settings.cpp
	${app_root}/src/services/test_pattern.cpp
	${app_root}/src/app/modes.cpp
)
target_include_directories(app PRIVATE ${app_root}/include)
target_compile_options(app PRIVATE -Wextra -Werror)
add_subdirectory(${app_root}/lib/qrcodegen qrcodegen)
target_link_libraries(app PRIVATE badge_qrcodegen)
