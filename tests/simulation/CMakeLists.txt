cmake_minimum_required(VERSION 3.20.0)

if(NOT DEFINED ZMK_SOURCE_DIR)
  message(FATAL_ERROR "Pass the pinned ZMK source directory as ZMK_SOURCE_DIR")
endif()
set(CONFIG_APPLICATION_DEFINED_SYSCALL true)
list(APPEND DTS_ROOT ${ZMK_SOURCE_DIR}/app)
list(APPEND SYSCALL_INCLUDE_DIRS ${ZMK_SOURCE_DIR}/app/include)
# With USJIS_TESTS the runner also passes -DZEPHYR_MODULES=<this repository> (there is no west workspace):
# the module zmk-feature-usjis adds its sources to `app` and places its
# listener before hid_listener.c.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(usjis_simulation)

# Compile the dependency's implementation verbatim. No firmware/core patches.
target_include_directories(app PRIVATE ${ZMK_SOURCE_DIR}/app/include)
zephyr_linker_sources(RODATA ${ZMK_SOURCE_DIR}/app/include/linker/zmk-events.ld)
if(USJIS_TESTS)
  target_sources(app PRIVATE src/usjis_main.c)
  target_sources(app PRIVATE ${ZMK_SOURCE_DIR}/app/src/events/endpoint_changed.c)
else()
  target_sources(app PRIVATE src/main.c)
  target_sources(app PRIVATE ${ZMK_SOURCE_DIR}/app/src/behaviors/behavior_mod_morph.c)
endif()
target_sources(app PRIVATE
  ${ZMK_SOURCE_DIR}/app/src/hid.c
  ${ZMK_SOURCE_DIR}/app/src/hid_listener.c
  ${ZMK_SOURCE_DIR}/app/src/event_manager.c
  ${ZMK_SOURCE_DIR}/app/src/events/keycode_state_changed.c
  ${ZMK_SOURCE_DIR}/app/src/behaviors/behavior_key_press.c
)
