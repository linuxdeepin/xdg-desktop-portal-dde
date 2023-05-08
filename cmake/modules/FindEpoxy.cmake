# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(PkgConfig QUIET)
pkg_check_modules(PKG_Epoxy QUIET epoxy)

set(Epoxy_DEFINITIONS "${PKG_Epoxy_CFLAGS}")

find_path(Epoxy_INCLUDE_DIRS NAMES epoxy/gl.h HINTS ${PKG_Epoxy_INCLUDEDIR} ${PKG_Epoxy_INCLUDE_DIRS})
find_library(Epoxy_LIBRARIES  NAMES epoxy      HINTS ${PKG_Epoxy_LIBDIR} ${PKG_Epoxy_LIBRARIES_DIRS})
find_file(Epoxy_GLX_HEADER NAMES epoxy/glx.h HINTS ${Epoxy_INCLUDE_DIRS} DOC "whether GLX is available")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Epoxy DEFAULT_MSG Epoxy_LIBRARIES Epoxy_INCLUDE_DIRS)

mark_as_advanced(Epoxy_INCLUDE_DIRS Epoxy_LIBRARIES Epoxy_HAS_GLX)
