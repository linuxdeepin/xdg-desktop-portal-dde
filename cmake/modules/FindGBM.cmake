# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
find_package(PkgConfig QUIET)
pkg_check_modules(PKG_GBM QUIET gbm)

set(GBM_DEFINITIONS "${PKG_GBM_CFLAGS_OTHER}")
set(GBM_VERSION "${PKG_GBM_VERSION}")

find_path(GBM_INCLUDE_DIRS
    NAMES
        gbm.h
    HINTS
        ${PKG_GBM_INCLUDE_DIRS}
)
find_library(GBM_LIBRARIES
    NAMES
        gbm
    HINTS
        ${PKG_GBM_LIBRARIES_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GBM
    FOUND_VAR
        GBM_FOUND
    REQUIRED_VARS
        GBM_LIBRARIES
        GBM_INCLUDE_DIRS
    VERSION_VAR
        GBM_VERSION
)

if(GBM_FOUND AND NOT TARGET GBM::GBM)
    add_library(GBM::GBM UNKNOWN IMPORTED)
    set_target_properties(GBM::GBM PROPERTIES
        IMPORTED_LOCATION "${GBM_LIBRARIES}"
        INTERFACE_COMPILE_OPTIONS "${GBM_DEFINITIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${GBM_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(GBM_LIBRARIES GBM_INCLUDE_DIRS)

include(FeatureSummary)
set_package_properties(GBM PROPERTIES
    URL "http://www.mesa3d.org"
    DESCRIPTION "Mesa gbm library."
)
