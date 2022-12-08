find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(
  Qt${QT_VERSION_MAJOR} CONFIG REQUIRED COMPONENTS
  Core
  Concurrent
  DBus
  Widgets)
#find_package(DtkCore)
#find_package(DtkGui)
#find_package(DtkWidget)
set(SRC
  xdg-desktop-portal-dde.cpp
  ddesktopportal.h
  ddesktopportal.cpp
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
  appchooser.h
  appchooser.cpp
  appchooserdialog.h
  appchooserdialog.cpp
  account.h
  account.cpp
  session.h
  session.cpp
  globalshortcut.h
  globalshortcut.cpp
  lockdown.h
  lockdown.cpp
  secret.h
  secret.cpp
  utils.h
  utils.cpp
  request.h
  request.cpp
  dbushelpers.h
)

add_executable(${PROJECT_NAME}
  ${SRC})

target_link_libraries(${PROJECT_NAME} PUBLIC
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::DBus
  Qt${QT_VERSION_MAJOR}::Concurrent)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
