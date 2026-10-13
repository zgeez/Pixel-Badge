# SPDX-License-Identifier: MIT
if(SB_CONFIG_BADGE_BLE)
  foreach(option BT BT_PERIPHERAL BT_SMP BT_SETTINGS BT_SMP_SC_ONLY
      BT_SMP_ENFORCE_MITM BT_SMP_APP_PAIRING_ACCEPT APP_BLE)
    set_config_bool(${DEFAULT_IMAGE} CONFIG_${option} y)
  endforeach()
  set_config_string(${DEFAULT_IMAGE} CONFIG_BT_DEVICE_NAME "Smart LED Badge")
  set_config_int(${DEFAULT_IMAGE} CONFIG_BT_MAX_CONN 1)
  set_config_int(${DEFAULT_IMAGE} CONFIG_BT_MAX_PAIRED 1)
  set_config_int(${DEFAULT_IMAGE} CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE 2048)
endif()
