# SPDX-License-Identifier: MIT
add_library(badge_qrcodegen STATIC qrcodegen.c)
target_include_directories(badge_qrcodegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(badge_qrcodegen PRIVATE zephyr_interface)
