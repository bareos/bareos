#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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

include_directories(${VIXDISKLIB_INCLUDE_DIRS})
get_filename_component(
  VIX_DISKLIB_LIBRARY_DIR ${VIXDISKLIB_LIBRARIES} DIRECTORY
)
link_directories(${VIX_DISKLIB_LIBRARY_DIR})

add_executable(bareos_vadp_dumper bareos_vadp_dumper.cc cbuf.cc copy_thread.cc)

target_link_options(bareos_vadp_dumper PRIVATE "-Wl,--disable-new-dtags")
target_link_libraries(bareos_vadp_dumper vixDiskLib pthread jansson)

if(VIXDISKLIBCREATEPARAMS_HAS_PHYSICALSECTORSIZE)
  target_compile_definitions(
    bareos_vadp_dumper PUBLIC VIXDISKLIBCREATEPARAMS_HAS_PHYSICALSECTORSIZE
  )
endif()

install(TARGETS bareos_vadp_dumper DESTINATION "${CMAKE_INSTALL_SBINDIR}")
install(
  FILES bareos_vadp_dumper_wrapper.sh
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${CMAKE_INSTALL_SBINDIR}"
)

include(DebugEdit)
