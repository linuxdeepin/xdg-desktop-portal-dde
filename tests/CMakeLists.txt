find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Test Widgets)

add_executable(test-settings-dbus-metatype
    test_settings_dbus_metatype.cpp
    ${CMAKE_SOURCE_DIR}/src/settings.cpp
    ${CMAKE_SOURCE_DIR}/src/settings.h
)

target_include_directories(test-settings-dbus-metatype PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test-settings-dbus-metatype PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Widgets
)

add_executable(test-screencast-session-lifecycle
    test_screencast_session_lifecycle.cpp
)

target_link_libraries(test-screencast-session-lifecycle PRIVATE
    Qt6::Core
    xdg-desktop-portal-dde-wayland
)

add_test(NAME screencast-session-lifecycle COMMAND test-screencast-session-lifecycle)

add_executable(test-screencast-source-selection
    test_screencast_source_selection.cpp
)

target_link_libraries(test-screencast-source-selection PRIVATE
    Qt6::Gui
    xdg-desktop-portal-dde-wayland
)

add_test(NAME screencast-source-selection COMMAND test-screencast-source-selection)
set_tests_properties(screencast-source-selection PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(test-pipewire-format-compatibility
    test_pipewire_format_compatibility.cpp
)

target_link_libraries(test-pipewire-format-compatibility PRIVATE
    xdg-desktop-portal-dde-wayland
)

add_test(NAME pipewire-format-compatibility COMMAND test-pipewire-format-compatibility)

add_executable(test-sealed-memfd
    test_sealed_memfd.cpp
    ${CMAKE_SOURCE_DIR}/src/wayland/sealedmemfd.cpp
)

target_include_directories(test-sealed-memfd PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME sealed-memfd COMMAND test-sealed-memfd)

add_test(NAME settings-dbus-metatype COMMAND test-settings-dbus-metatype)
set_tests_properties(settings-dbus-metatype PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(test-global-shortcuts-contract
    test_global_shortcuts_contract.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcut.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcut.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcuttypes.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcuttypes.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcutrequest.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcutrequest.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcutsession.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcutsession.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcutscontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcutscontroller.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcutsmanagement.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcutsmanagement.h
)

target_include_directories(test-global-shortcuts-contract PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(test-global-shortcuts-contract PRIVATE
    GLOBAL_SHORTCUTS_XML_PATH="${CMAKE_SOURCE_DIR}/misc/org.freedesktop.impl.portal.GlobalShortcuts.xml"
    GLOBAL_SHORTCUTS_MANAGEMENT_XML_PATH="${CMAKE_SOURCE_DIR}/misc/org.deepin.dde.impl.portal.GlobalShortcutsManagement1.xml"
    GLOBAL_SHORTCUTS_PROVIDER_XML_PATH="${CMAKE_SOURCE_DIR}/misc/org.deepin.dde.GlobalShortcutsProvider1.xml"
    DDE_PORTAL_DECLARATION_PATH="${CMAKE_SOURCE_DIR}/misc/dde.portal"
)

target_link_libraries(test-global-shortcuts-contract PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
)

add_test(NAME global-shortcuts-contract COMMAND test-global-shortcuts-contract)

qt_add_executable(test-global-shortcuts-controller
    test_global_shortcuts_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcut.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcut.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcuttypes.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcuttypes.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcutrequest.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcutrequest.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcutsession.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcutsession.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcutscontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcutscontroller.h
    ${CMAKE_SOURCE_DIR}/src/globalshortcutsmanagement.cpp
    ${CMAKE_SOURCE_DIR}/src/globalshortcutsmanagement.h
)

target_include_directories(test-global-shortcuts-controller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test-global-shortcuts-controller PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Test
)

find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
add_test(NAME global-shortcuts-controller
         COMMAND ${DBUS_RUN_SESSION_EXECUTABLE} -- $<TARGET_FILE:test-global-shortcuts-controller>)
