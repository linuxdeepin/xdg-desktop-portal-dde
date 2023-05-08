configure_file(org.freedesktop.impl.portal.desktop.dde.cmake.in org.freedesktop.impl.portal.desktop.dde.service @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.impl.portal.desktop.dde.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
install(FILES dde.portal DESTINATION ${DATA_INSTALL_DIR}/xdg-desktop-portal/portals)
