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

include(systemdservice)
if(${SYSTEMD_FOUND})
  set(HAVE_SYSTEMD 1)
endif()

option(ENABLE_PYTHON "Enable Python support" ON)

if(NOT ENABLE_PYTHON)
  set(HAVE_PYTHON 0)
  set(Python3_FOUND 0)
  # Python Plugins currently cannot be built for Solaris
elseif(${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
  set(HAVE_PYTHON 0)
  set(Python3_FOUND 0)

else()
  find_package(Python3 COMPONENTS Interpreter Development Development.Module)

  set(Python3_VERSION_MAJOR
      ${Python3_VERSION_MAJOR}
      PARENT_SCOPE
  )

  set(Python3_VERSION_MINOR
      ${Python3_VERSION_MINOR}
      PARENT_SCOPE
  )

  if(${Python3_FOUND})
    set(HAVE_PYTHON 1)
  endif()

  if(${Python3_FOUND})
    set(PYTHON_EXECUTABLE
        ${Python3_EXECUTABLE}
        PARENT_SCOPE
    )
    set(Python3_EXECUTABLE
        ${Python3_EXECUTABLE}
        PARENT_SCOPE
    )
    execute_process(
      COMMAND ${Python3_EXECUTABLE}
              ${CMAKE_CURRENT_SOURCE_DIR}/cmake/get_python_compile_settings.py
      OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/py3settings.cmake
    )
    include(${CMAKE_CURRENT_BINARY_DIR}/py3settings.cmake)
  endif()
endif()

include(FindPostgreSQL)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(OPENSSL_USE_STATIC_LIBS 1)
endif()

find_package(OpenSSL 1.0.2 REQUIRED)
set(HAVE_OPENSSL 1)

include(BareosFindLibraryAndHeaders)

bareosfindlibraryandheaders(
  "vixDiskLib" "vixDiskLib.h"
  "/usr/lib/vmware-vix-disklib-distrib;/usr/lib/vmware-vix-disklib"
)

# check for structmember physicalSectorSize in struct VixDiskLibCreateParams
if(VIXDISKLIB_FOUND)
  include(CheckStructHasMember)
  check_struct_has_member(
    "VixDiskLibCreateParams" physicalSectorSize
    ${VIXDISKLIB_INCLUDE_DIRS}/vixDiskLib.h
    VIXDISKLIBCREATEPARAMS_HAS_PHYSICALSECTORSIZE
  )
endif()

if(VIXDISKLIB_FOUND)
  if((NOT DEFINED vmware_server)
     OR (NOT DEFINED vmware_user)
     OR (NOT DEFINED vmware_password)
     OR (NOT DEFINED vmware_datacenter)
     OR (NOT DEFINED vmware_folder)
  )
    string(
      CONCAT MSG
             "VMware Vix Disklib was found. To enable the vmware plugin test, "
             "please provide the required information:"
             "example:"
             " -Dvmware_user=Administrator@vsphere.local "
             " -Dvmware_password=\"@one2threeBareos\" "
             " -Dvmware_vm_name=testvm1 "
             " -Dvmware_datacenter=mydc1 "
             " -Dvmware_folder=\"/webservers\" "
             " -Dvmware_server=139.178.73.195"
    )
    message(WARNING ${MSG})
  else()
    set(enable_vmware_test
        1
        PARENT_SCOPE
    )
  endif()
elseif(
  (DEFINED vmware_server)
  OR (DEFINED vmware_user)
  OR (DEFINED vmware_password)
  OR (DEFINED vmware_datacenter)
  OR (DEFINED vmware_folder)
)
  message(
    FATAL_ERROR
      "vmware options were set but VMware Vix Disklib was not found. Cannot run vmware tests."
  )
endif()
if(NOT MSVC)
  bareosfindlibraryandheaders("pthread" "pthread.h" "")
endif()
bareosfindlibraryandheaders("cap" "sys/capability.h" "")
bareosfindlibraryandheaders("gfapi" "glusterfs/api/glfs.h" "")

bareosfindlibraryandheaders("pam" "security/pam_appl.h" "")

option(ENABLE_LZO "Enable LZO support" ON)
if(ENABLE_LZO)
  bareosfindlibraryandheaders("lzo2" "lzo/lzoconf.h" "")
  if(${LZO2_FOUND})
    set(HAVE_LZO 1)
    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
      set(LZO2_LIBRARIES "${HOMEBREW_PREFIX}/opt/lzo/lib/liblzo2.a")
    endif()
  endif()
endif()

include(BareosFindLibrary)

bareosfindlibraryandheaders("tirpc" "rpc/rpc.h" "/usr/include/tirpc")
bareosfindlibrary("util")
bareosfindlibrary("dl")
bareosfindlibrary("acl")
if(NOT ${CMAKE_CXX_COMPILER_ID} MATCHES SunPro)
  find_package(GTest 1.8 CONFIG)
endif()

if(${HAVE_CAP})
  set(HAVE_LIBCAP 1)
endif()

find_package(ZLIB)
if(${ZLIB_FOUND})
  set(HAVE_LIBZ 1)
  # yes its actually libz vs zlib ...
  set(HAVE_ZLIB_H 1)
endif()

find_package(Readline)

option(ENABLE_JANSSON "Build with Jansson library (required for director)" ON)
if(ENABLE_JANSSON)
  find_package(Jansson)
endif()

option(ENABLE_GRPC "Build with grpc support" OFF)

if(ENABLE_GRPC)
  find_package(Protobuf 3.12.0 REQUIRED)
  find_package(gRPC REQUIRED)
endif()

if(NOT MSVC)
  include(thread)
else()
  find_package(pthread)
endif()
if(MSVC)
  find_package(Intl)
endif()
