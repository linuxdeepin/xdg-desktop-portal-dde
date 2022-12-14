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
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set_source_files_properties(
                      ${CMAKE_SOURCE_DIR}/xml/org.freedesktop.Notifications.xml
                      PROPERTIES
                      CLASSNAME
                        NotificationService
                      NO_NAMESPACE
                        OFF
)
qt5_add_dbus_interface(NotificationInterface
                      ${CMAKE_SOURCE_DIR}/xml/org.freedesktop.Notifications.xml
                      notification_interface
)

set(SRC
  ${NotificationInterface}
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
  appchooserdelegate.h
  appchooserdelegate.cpp
  appchoosermodel.h
  appchoosermodel.cpp
  iteminfo.h
  iteminfo.cpp
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
  request.h
  request.cpp
  dbushelpers.h
  utils.h
  utils.cpp
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
