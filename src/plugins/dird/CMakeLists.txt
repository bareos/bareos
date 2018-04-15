#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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


IF(HAVE_WIN32)
   include_directories(
      ${CMAKE_SOURCE_DIR}/src/win32/generic
      ${CMAKE_SOURCE_DIR}/src/win32/dird
      ${CMAKE_SOURCE_DIR}/src/win32/plugins/python/include
      ../../include
      ../../dird
      ../..)
   link_directories(${CMAKE_SOURCE_DIR}/src/win32/plugins/python/lib/${WINDOWS_BITS})
   add_definitions(-DMS_WIN${WINDOWS_BITS})
ELSE()
   include_directories(../.. . ../../include ../../dird)
ENDIF()

include_directories(
   ${PTHREAD_INCLUDE_DIRS}
   ${FASTLZ_INCLUDE_DIRS}
   ${ZLIB_INCLUDE_DIRS}
   ${ACL_INCLUDE_DIRS}
   ${LZO2_INCLUDE_DIRS}
   ${CAP_INCLUDE_DIRS}
   ${WRAP_INCLUDE_DIRS}
)
include_directories(BEFORE ${OPENSSL_INCLUDE_DIRS} ${Readline_INCLUDE_DIR} )

MESSAGE( STATUS "src/plugins/dird build: " )
get_property(got_d_include_dirs DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
foreach(_dir IN LISTS got_d_include_dirs)
   message(STATUS "  -I ${_dir}")
endforeach()


IF(HAVE_PYTHON)
   add_library(python-dir MODULE python-dir.cc)
   # do not prefix with "lib"
   set_target_properties(python-dir PROPERTIES PREFIX "")
   INSTALL(TARGETS python-dir DESTINATION ${plugindir})
   target_link_libraries(python-dir ${PYTHON_LIBRARIES} bareos)


   set (PYFILES bareos-dir.py.template
      bareos-dir-class-plugin.py
      bareos_dir_consts.py
      BareosDirPluginBaseclass.py
      BareosDirWrapper.py)

   INSTALL(FILES ${PYFILES} DESTINATION ${plugindir})
ENDIF()
