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

if(INCUS)
  set(INCUS_VM_BASE_NAME
      "bareos-test-vm"
      CACHE STRING "base vm image for the incus test"
  )
  set(INCUS_CT_BASE_NAME
      "bareos-test-ct"
      CACHE STRING "base container image for the incus test"
  )

  # incus only allows alphanumeric characters + hyphens in names but only a max
  # of 63 characters; we leave 13 chars to the tests so we can only create a
  # name of length 50

  set(MAX_LENGTH 50)

  if(DEFINED CIUID)
    string(REGEX REPLACE "[^A-Za-z0-9]" "-" INCUS_VM_NAME
                         "${INCUS_VM_BASE_NAME}-${CIUID}"
    )
    string(REGEX REPLACE "[^A-Za-z0-9]" "-" INCUS_CT_NAME
                         "${INCUS_CT_BASE_NAME}-${CIUID}"
    )

    string(LENGTH "${INCUS_VM_NAME}" VM_LENGTH)
    string(LENGTH "${INCUS_CT_NAME}" CT_LENGTH)

    if(VM_LENGTH GREATER MAX_LENGTH)
      message(
        "incus: Generated vm name ${INCUS_VM_NAME} is too long.  Generating fallback name"
      )

      math(EXPR MAX_SUB_LEN "${MAX_LENGTH} - 3") # VM-
      string(SUBSTRING "${CIUID}" 0 MAX_SUB_LEN USED_CIUID)
      string(REGEX REPLACE "[^A-Za-z0-9]" "-" INCUS_VM_NAME "VM-${USED_CIUID}")
    endif()

    if(CT_LENGTH GREATER MAX_LENGTH)
      message(
        "incus: Generated ct name ${INCUS_CT_NAME} is too long.  Generating fallback name"
      )

      math(EXPR MAX_SUB_LEN "${MAX_LENGTH} - 3") # CT-
      string(SUBSTRING "${CIUID}" 0 MAX_SUB_LEN USED_CIUID)
      string(REGEX REPLACE "[^A-Za-z0-9]" "-" INCUS_CT_NAME "CT-${USED_CIUID}")
    endif()

  else()
    string(REGEX REPLACE "[^A-Za-z0-9]" "-" INCUS_VM_NAME
                         "${INCUS_VM_BASE_NAME}"
    )
    string(REGEX REPLACE "[^A-Za-z0-9]" "-" INCUS_CT_NAME
                         "${INCUS_CT_BASE_NAME}"
    )
  endif()

  message("incus: using ${INCUS_VM_NAME} / ${INCUS_CT_NAME}")
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME} NO_GRPC)
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT "necessary tools missing"
  )
endif()
