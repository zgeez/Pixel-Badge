# SPDX-License-Identifier: Apache-2.0
# Modified from Zephyr's raytac_mdbt53_db_40 board for this board. Programmed
# over SWD through the nRF5340 DK debug out or another J-Link (docs/bringup.md).

if(CONFIG_BOARD_SMART_LED_BADGE_NRF5340_CPUAPP)
  board_runner_args(jlink "--device=nrf5340_xxaa_app" "--speed=4000")
elseif(CONFIG_BOARD_SMART_LED_BADGE_NRF5340_CPUNET)
  board_runner_args(jlink "--device=nrf5340_xxaa_net" "--speed=4000")
endif()

include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
