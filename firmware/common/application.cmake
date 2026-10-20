# SPDX-License-Identifier: MIT
# Shared badge application; included after find_package(Zephyr).
if(NOT DEFINED BADGE_MAIN_SOURCE)
    set(BADGE_MAIN_SOURCE ${BADGE_COMMON_DIR}/src/main.cpp)
    set(BADGE_USE_PROTOTYPE_LOOP TRUE)
endif()
target_sources(app PRIVATE
    ${BADGE_MAIN_SOURCE}
	${BADGE_COMMON_DIR}/src/app/bringup.cpp
	${BADGE_COMMON_DIR}/src/app/power_diag.cpp
	${BADGE_COMMON_DIR}/src/app/matrix_diag.cpp
	${BADGE_COMMON_DIR}/src/services/power_names.cpp
	${BADGE_COMMON_DIR}/src/services/test_pattern.cpp
    ${BADGE_COMMON_DIR}/src/services/artwork_store.cpp
	${BADGE_COMMON_DIR}/src/services/qr.cpp
	${BADGE_COMMON_DIR}/src/services/ndef.cpp
	${BADGE_COMMON_DIR}/src/services/ntag.cpp
	${BADGE_COMMON_DIR}/src/platform/nfc_transport.cpp
)
add_subdirectory(${BADGE_COMMON_DIR}/lib/qrcodegen ${CMAKE_CURRENT_BINARY_DIR}/qrcodegen)
target_link_libraries(app PRIVATE badge_qrcodegen)
target_sources_ifdef(CONFIG_APP_NFC app PRIVATE ${BADGE_COMMON_DIR}/src/app/nfc.cpp)
target_sources_ifdef(CONFIG_APP_NFC app PRIVATE ${BADGE_COMMON_DIR}/src/services/url_update.cpp)
target_sources_ifdef(CONFIG_APP_NFC app PRIVATE ${BADGE_COMMON_DIR}/src/services/nfc_session.cpp)
target_sources_ifdef(CONFIG_APP_BLE app PRIVATE ${BADGE_COMMON_DIR}/src/app/ble.cpp ${BADGE_COMMON_DIR}/src/services/ble_protocol.cpp)
target_sources_ifdef(CONFIG_APP_AUDIO app PRIVATE ${BADGE_COMMON_DIR}/src/app/audio_runtime.cpp
 ${BADGE_COMMON_DIR}/src/platform/audio_crypto.cpp ${BADGE_COMMON_DIR}/src/services/audio.cpp ${BADGE_COMMON_DIR}/src/services/adpcm.cpp
 ${BADGE_COMMON_DIR}/src/services/recording.cpp)
target_sources_ifdef(CONFIG_APP_MOTION app PRIVATE ${BADGE_COMMON_DIR}/src/app/motion_runtime.cpp ${BADGE_COMMON_DIR}/src/services/motion.cpp)
target_sources_ifdef(CONFIG_DISPLAY app PRIVATE ${BADGE_COMMON_DIR}/src/app/display_demo.cpp)
target_sources_ifdef(CONFIG_APP_AUDIO_SCREENING app PRIVATE
	${BADGE_COMMON_DIR}/src/app/audio_screening.cpp ${BADGE_COMMON_DIR}/src/services/audio.cpp ${BADGE_COMMON_DIR}/src/services/buttons.cpp)
if(CONFIG_APP_PROTOTYPE)
	# Read sysbuild overrides too; a link change must not retain an old cache value.
	zephyr_get(BADGE_QR_LINK)
	zephyr_get(BADGE_QR_ENV_FILE)
	if(NOT DEFINED BADGE_QR_LINK)
		set(BADGE_QR_LINK "PORTFOLIO")
	endif()
	if(NOT DEFINED BADGE_QR_ENV_FILE)
		set(BADGE_QR_ENV_FILE "${BADGE_REPOSITORY_ROOT}/.env")
	endif()
	file(TO_CMAKE_PATH "${BADGE_QR_ENV_FILE}" BADGE_QR_ENV_FILE)
	set(default_url_script "${BADGE_REPOSITORY_ROOT}/scripts/qr/configure_default_url.py")
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
		${BADGE_COMMON_DIR}/src/services/settings.cpp
		${BADGE_COMMON_DIR}/src/services/buttons.cpp
		${BADGE_COMMON_DIR}/src/services/boot_guard.cpp
		${BADGE_COMMON_DIR}/src/services/display_health.cpp
		${BADGE_COMMON_DIR}/src/services/text.cpp
		${BADGE_COMMON_DIR}/src/services/content.cpp
		${BADGE_COMMON_DIR}/src/platform/settings_store.cpp
		${BADGE_COMMON_DIR}/src/platform/boot_retention.cpp
		${BADGE_COMMON_DIR}/src/app/modes.cpp
	)
    if(BADGE_USE_PROTOTYPE_LOOP)
        target_sources(app PRIVATE ${BADGE_COMMON_DIR}/src/app/prototype.cpp)
    endif()
endif()
if(CONFIG_APP_QR_SCREENING)
	if(CONFIG_APP_QR_CODES_FILE)
		set(qr_codes ${CONFIG_APP_QR_CODES_FILE})
	else()
		set(qr_codes ${BADGE_COMMON_DIR}/src/services/qr_test_codes.cpp)
	endif()
	target_sources(app PRIVATE ${BADGE_COMMON_DIR}/src/app/qr_screening.cpp ${qr_codes})
endif()

target_include_directories(app PRIVATE ${BADGE_COMMON_DIR}/include)

# Strict warnings for application-owned sources only. These options apply to
# the `app` target and do not change how Zephyr or third-party modules build.
target_compile_options(app PRIVATE -Wextra -Werror)
