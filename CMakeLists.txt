#
#   BAREOS - Backup Archiving REcovery Open Sourced
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

cmake_minimum_required(VERSION 3.0)
project(bareos-vmware)

INCLUDE(GNUInstallDirs)

add_subdirectory(vadp_dumper)
add_subdirectory(vmware_cbt_tool)
add_subdirectory(vmware_plugin)
#add_subdirectory(vmware-vix-disklib)


#
# commented, as it has not been used for inherited directories.
#
# set INSTALL_LIB_DIR automatically to lib or lib64 automatically
# Taken from  https://cmake.org/pipermail/cmake/2013-July/055374.html

#get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
#if ("${LIB64}" STREQUAL "TRUE")
#        set(LIBSUFFIX 64)
#else()
#        set(LIBSUFFIX "")
#endif()
#set(INSTALL_LIB_DIR  "lib${LIBSUFFIX}" CACHE PATH "Installation directory for libraries")
#mark_as_advanced(INSTALL_LIB_DIR)

