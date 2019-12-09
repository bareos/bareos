#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2019 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

set(BAREOSFIND_SRCS
    acl.cc
    attribs.cc
    bfile.cc
    create_file.cc
    drivetype.cc
    enable_priv.cc
    find_one.cc
    find.cc
    fstype.cc
    hardlink.cc
    match.cc
    mkpath.cc
    shadowing.cc
    xattr.cc
)

if(HAVE_WIN32)
  list(APPEND BAREOSFIND_SRCS ../win32/findlib/win32.cc)
endif()

include_directories(${OPENSSL_INCLUDE_DIR})

add_library(bareosfind SHARED ${BAREOSFIND_SRCS})

target_link_libraries(bareosfind bareos ${ACL_LIBS})

install(
  TARGETS bareosfind
  DESTINATION ${libdir}
  COMPONENT filedaemon
)

set_target_properties(
  bareosfind
  PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
             SOVERSION "${BAREOS_VERSION_MAJOR}"
             DEFINE_SYMBOL "BUILDING_DLL"
)
if(HAVE_WIN32)
  set_target_properties(bareosfind PROPERTIES DEFINE_SYMBOL "BUILDING_DLL")
endif()

if(NOT HAVE_WIN32 AND GTEST_FOUND)
  # add_subdirectory(unittests)
endif()
