# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20.0)

# Project drivers (Kconfig, devicetree bindings, sources) as a Zephyr module.
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(smart_led_badge LANGUAGES C CXX)

target_sources(app PRIVATE
	src/main.cpp
	src/app/bringup.cpp
	src/services/test_pattern.cpp
	src/services/qr.cpp
	src/services/ndef.cpp
	src/services/ntag.cpp
	src/platform/nfc_transport.cpp
)
add_subdirectory(lib/qrcodegen)
target_link_libraries(app PRIVATE badge_qrcodegen)
target_sources_ifdef(CONFIG_APP_NFC app PRIVATE src/app/nfc.cpp)
target_sources_ifdef(CONFIG_APP_NFC app PRIVATE src/services/url_update.cpp)
target_sources_ifdef(CONFIG_APP_BLE app PRIVATE src/app/ble.cpp src/services/ble_protocol.cpp)
target_sources_ifdef(CONFIG_DISPLAY app PRIVATE src/app/display_demo.cpp)
if(CONFIG_APP_PROTOTYPE)
	# Read sysbuild overrides too; a link change must not retain an old cache value.
	zephyr_get(BADGE_QR_LINK)
	zephyr_get(BADGE_QR_ENV_FILE)
	if(NOT DEFINED BADGE_QR_LINK)
		set(BADGE_QR_LINK "PORTFOLIO")
	endif()
	if(NOT DEFINED BADGE_QR_ENV_FILE)
		set(BADGE_QR_ENV_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../.env")
	endif()
	file(TO_CMAKE_PATH "${BADGE_QR_ENV_FILE}" BADGE_QR_ENV_FILE)
	set(default_url_script "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/qr/configure_default_url.py")
	set(default_url_header "${CMAKE_CURRENT_BINARY_DIR}/generated/badge/default_url.hpp")
	# Watch creation/deletion as well as edits without making a missing .env
	# an always-dirty Ninja dependency in public/CI builds.
	file(GLOB qr_env_files CONFIGURE_DEPENDS LIST_DIRECTORIES false "${BADGE_QR_ENV_FILE}")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
		${qr_env_files} "${default_url_script}")
	execute_process(
		COMMAND ${PYTHON_EXECUTABLE} "${default_url_script}"
			--env-file "${BADGE_QR_ENV_FILE}" --link "${BADGE_QR_LINK}"
			--out-header "${default_url_header}"
		RESULT_VARIABLE default_url_result
	)
	if(NOT default_url_result EQUAL 0)
		message(FATAL_ERROR "Cannot configure the prototype QR default")
	endif()
	target_include_directories(app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
	target_compile_definitions(app PRIVATE BADGE_CONFIGURED_DEFAULT_URL=1)
	target_sources(app PRIVATE
		src/services/settings.cpp
		src/services/buttons.cpp
		src/services/boot_guard.cpp
		src/services/display_health.cpp
		src/services/text.cpp
		src/services/content.cpp
		src/platform/settings_store.cpp
		src/platform/boot_retention.cpp
		src/app/modes.cpp
		src/app/prototype.cpp
	)
endif()
if(CONFIG_APP_QR_SCREENING)
	if(CONFIG_APP_QR_CODES_FILE)
		set(qr_codes ${CONFIG_APP_QR_CODES_FILE})
	else()
		set(qr_codes src/services/qr_test_codes.cpp)
	endif()
	target_sources(app PRIVATE src/app/qr_screening.cpp ${qr_codes})
endif()

target_include_directories(app PRIVATE include)

# Strict warnings for application-owned sources only. These options apply to
# the `app` target and do not change how Zephyr or third-party modules build.
target_compile_options(app PRIVATE -Wextra -Werror)
