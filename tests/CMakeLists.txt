find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Widgets)
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
)

target_include_directories(test-screenshot-interface PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test-screenshot-interface PRIVATE
    xdg-desktop-portal-dde-wayland
    Qt6::Core
    Qt6::DBus
)

add_test(NAME screenshot-interface COMMAND test-screenshot-interface)
