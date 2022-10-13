cmake_minimum_required(VERSION 3.16)

project(xdg-desktop-protal-dde
  VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX /usr)
endif ()
include(GNUInstallDirs)


add_subdirectory(src)
