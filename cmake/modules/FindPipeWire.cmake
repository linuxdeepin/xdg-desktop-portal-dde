# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
find_package(PkgConfig QUIET)
pkg_check_modules(PKG_PipeWire QUIET libpipewire-0.2 libpipewire-0.3)

set(PipeWire_DEFINITIONS "${PKG_PipeWire_CFLAGS_OTHER}")
set(PipeWire_VERSION "${PKG_PipeWire_VERSION}")

find_path(PipeWire_INCLUDE_DIRS
    NAMES
        pipewire/pipewire.h
    HINTS
        ${PKG_PipeWire_INCLUDE_DIRS}
)

find_library(PipeWire_LIBRARIES
    NAMES
        pipewire-0.2 pipewire-0.3
    HINTS
        ${PKG_PipeWire_LIBRARIES_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PipeWire
    FOUND_VAR
        PipeWire_FOUND
    REQUIRED_VARS
        PipeWire_LIBRARIES
        PipeWire_INCLUDE_DIRS
    VERSION_VAR
        PipeWire_VERSION
)

if(PipeWire_FOUND AND NOT TARGET PipeWire::PipeWire)
    add_library(PipeWire::PipeWire UNKNOWN IMPORTED)
    set_target_properties(PipeWire::PipeWire PROPERTIES
        IMPORTED_LOCATION "${PipeWire_LIBRARIES}"
        INTERFACE_COMPILE_OPTIONS "${PipeWire_DEFINITIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${PipeWire_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(PipeWire_LIBRARIES PipeWire_INCLUDE_DIRS)

include(FeatureSummary)
set_package_properties(PipeWire PROPERTIES
    URL "http://www.pipewire.org"
    DESCRIPTION "PipeWire - multimedia processing"
)
