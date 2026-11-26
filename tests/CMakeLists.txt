find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Test Widgets)
find_package(Dtk6 REQUIRED COMPONENTS Widget)

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

add_executable(test-screenshot-image
    test_screenshot_image.cpp
    ${CMAKE_SOURCE_DIR}/src/wayland/screenshotimage.cpp
    ${CMAKE_SOURCE_DIR}/src/wayland/screenshotimage.h
)

target_include_directories(test-screenshot-image PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test-screenshot-image PRIVATE
    Qt6::Core
    Qt6::Gui
)

add_test(NAME screenshot-image COMMAND test-screenshot-image)

add_executable(test-access-dialog
    test_access_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/accessdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/accessdialog.h
    ${CMAKE_SOURCE_DIR}/src/accessdialogtypes.h
)

target_include_directories(test-access-dialog PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test-access-dialog PRIVATE
    Dtk6::Widget
    Qt6::Core
    Qt6::DBus
    Qt6::Widgets
)

add_test(NAME access-dialog COMMAND test-access-dialog)
set_tests_properties(access-dialog PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(test-access-dbus-types
    test_access_dbus_types.cpp
    ${CMAKE_SOURCE_DIR}/src/accessdialogtypes.cpp
    ${CMAKE_SOURCE_DIR}/src/accessdialogtypes.h
)

target_include_directories(test-access-dbus-types PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test-access-dbus-types PRIVATE
    Qt6::Core
    Qt6::DBus
)

add_test(NAME access-dbus-types COMMAND test-access-dbus-types)

add_executable(test-screenshot-interface
    test_screenshot_interface.cpp
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

target_include_directories(test-screenshot-interface PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test-screenshot-interface PRIVATE
    xdg-desktop-portal-dde-wayland
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
)

add_test(NAME screenshot-interface COMMAND test-screenshot-interface)
