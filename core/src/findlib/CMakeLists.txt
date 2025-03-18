#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2025 Bareos GmbH & Co. KG
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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

add_library(bareosfind SHARED)
add_library(Bareos::Findlib ALIAS bareosfind)
target_sources(
  bareosfind
  PRIVATE acl.cc
          attribs.cc
          bfile.cc
          create_file.cc
          drivetype.cc
          enable_priv.cc
          find_one.cc
          find.cc
          fstype.cc
          match.cc
          mkpath.cc
          shadowing.cc
          xattr.cc
)
if(HAVE_WIN32)
  target_sources(bareosfind PRIVATE win32.cc win32.h)
endif()

target_link_libraries(bareosfind PRIVATE Bareos::Lib ${ACL_LIBRARIES})

if(HAVE_DARWIN_OS)
  # dev_t is signed on Darwin
  target_compile_options(bareosfind PRIVATE -Wno-sign-compare)
endif()

set_target_properties(
  bareosfind PROPERTIES VERSION "${BAREOS_NUMERIC_VERSION}"
                        SOVERSION "${BAREOS_VERSION_MAJOR}"
)
if(HAVE_WIN32)
  set_target_properties(bareosfind PROPERTIES DEFINE_SYMBOL "BUILDING_DLL")
endif()

install(
  TARGETS bareosfind
  DESTINATION ${libdir}
  COMPONENT filedaemon
)

include(DebugEdit)
