# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2025-2026 Benjamin Somers, IMT Atlantique
# Copyright (C) 2025-2026 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)

find_program(INCUS incus)

# The following dependencies are needed to create a tiny Alpine image for Incus
find_program(GUESTFISH guestfish)
find_program(QEMUIMG qemu-img)
find_program(VIRTRESIZE virt-resize)

# We use LZMA compression in our tests
find_program(LZMA lzma)

if(NOT ENABLE_INCUS_PLUGIN)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT "no incus plugin"
  )
  return()
endif()

if(NOT TARGET python3-fd)
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT "no python plugin"
  )
  return()
endif()

set(Python3_VERSION "${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}")
if(Python3_VERSION VERSION_LESS "3.13")
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT "python < 3.13"
  )
  return()
endif()

if(DEFINED ENV{BUILDID})
  message("have buildid $ENV{BUILDID}")
else()
  message(FATAL_ERROR "have no buildid $ENV{BUILDID}")
endif()

if(INCUS
   AND GUESTFISH
   AND LZMA
   AND QEMUIMG
   AND VIRTRESIZE
)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT "necessary tools missing"
  )
endif()
