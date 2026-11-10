find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus Gui Test Widgets)

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
