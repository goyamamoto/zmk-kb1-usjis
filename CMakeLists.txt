# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_USJIS)
  set(ZMK_USJIS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  # The sources join the ZMK `app` library so that they see its private
  # include directories and so that the listener's position in the
  # .event_subscription section can be controlled (see below).
  target_include_directories(app PRIVATE ${ZMK_USJIS_DIR}/include)
  target_sources(app PRIVATE
    ${ZMK_USJIS_DIR}/src/usjis_resolver.c
    ${ZMK_USJIS_DIR}/src/usjis.c
    ${ZMK_USJIS_DIR}/src/behavior_usjis.c
  )

  # Listener order is link order. Modules are processed before the application
  # adds its own sources, so usjis.c would come first and run before hold-tap
  # and key repeat. After the application directory has been processed, move
  # usjis.c to just before hid_listener.c (architecture section 6). The
  # startup check in usjis.c verifies the result.
  function(zmk_usjis_place_listener)
    get_property(listener GLOBAL PROPERTY ZMK_USJIS_LISTENER_SOURCE)
    get_target_property(sources app SOURCES)
    list(LENGTH sources n)
    message(STATUS "zmk-feature-usjis: placing ${listener} among ${n} app sources")
    list(REMOVE_ITEM sources ${listener})
    set(index -1)
    set(i 0)
    foreach(source ${sources})
      if(source MATCHES "/hid_listener\\.c$")
        set(index ${i})
      endif()
      math(EXPR i "${i} + 1")
    endforeach()
    if(index EQUAL -1)
      message(FATAL_ERROR "zmk-feature-usjis: hid_listener.c not found among app sources")
    endif()
    list(INSERT sources ${index} ${listener})
    set_property(TARGET app PROPERTY SOURCES "${sources}")
    message(STATUS "zmk-feature-usjis: listener at index ${index} before hid_listener.c")
  endfunction()
  set_property(GLOBAL PROPERTY ZMK_USJIS_LISTENER_SOURCE ${ZMK_USJIS_DIR}/src/usjis.c)
  cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL zmk_usjis_place_listener)
endif()
