find_package(Qt6 REQUIRED COMPONENTS Core DBus Widgets)

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
