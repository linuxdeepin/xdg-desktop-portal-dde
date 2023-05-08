find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(
  Qt${QT_VERSION_MAJOR} CONFIG REQUIRED COMPONENTS
  Core
  Concurrent
  DBus
  Widgets)
find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED
    I18n
    Declarative
    Notifications
    Wayland
)

find_package(KF5I18n CONFIG REQUIRED)
find_package(Wayland 1.15 REQUIRED COMPONENTS Client)
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
  screencast.cpp
  screencasting.cpp
  screencastwidget.cpp
  screenchooserdialog.cpp
  remotedesktop.cpp
  remotedesktopdialog.cpp
  waylandintegration.cpp
  wayland-zkde-screencast-unstable-v1-protocol.c
  qwayland-zkde-screencast-unstable-v1.cpp)

QT5_WRAP_UI(SRC
  screenchooserdialog.ui
  remotedesktopdialog.ui)


add_executable(${PROJECT_NAME}
  ${SRC})

target_link_libraries(${PROJECT_NAME} PUBLIC
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::DBus
  Qt${QT_VERSION_MAJOR}::Concurrent
  KF5::I18n
  KF5::WaylandClient
  KF5::Notifications
  Wayland::Client
  epoxy
  pipewire-0.2
  gbm)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
