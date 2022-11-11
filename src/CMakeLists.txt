set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(
  Qt${QT_VERSION_MAJOR} CONFIG REQUIRED COMPONENTS
  Core
  Concurrent
  DBus
  Widgets
  WaylandClient)
find_package(PkgConfig REQUIRED)

pkg_check_modules(wayland-client REQUIRED wayland-client)
include(${CMAKE_SOURCE_DIR}/cmake/DWaylandScanner.cmake)

set(scannerSRC)
DWaylandScannerClientHEAD(
  zkde-screencast-unstable-v1
  ${CMAKE_SOURCE_DIR}/protocols/zkde-screencast-unstable-v1.xml
  scannerSRC)

set(SRC
  xdg-desktop-protal-dde.cpp
  ddesktopprotal.h
  ddesktopprotal.cpp
  screenshot.h
  screenshot.cpp
  screencast.h
  screencast.cpp
  background.h
  background.cpp
  filechooser.h
  filechooser.cpp
  wallpaper.h
  wallpaper.cpp
  notification.h
  notification.cpp
  settings.h
  settings.cpp
  inhibit.h
  inhibit.cpp
  account.h
  account.cpp
  session.h
  session.cpp
  globalshotcut.h
  globalshotcut.cpp
  lockdown.h
  lockdown.cpp
  secret.h
  secret.cpp
)

add_executable(${PROJECT_NAME}
  ${SRC}
  ${scannerSRC})

target_link_libraries(${PROJECT_NAME} PUBLIC
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::DBus
  Qt${QT_VERSION_MAJOR}::Concurrent
  Qt${QT_VERSION_MAJOR}::WaylandClient
  ${wayland-client_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
