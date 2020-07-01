#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2020 Bareos GmbH & Co. KG
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

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  # make sure we get python 2 not 3
  set(Python_ADDITIONAL_VERSIONS 2.5 2.6 2.7 2.8 2.9)
  find_package(PythonInterp)
  include(FindPythonLibs)

  if(${PYTHONLIBS_FOUND})
    set(HAVE_PYTHON 1)
  endif()

  include(FindPostgreSQL)
endif()

include(CMakeUserFindMySQL)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(OPENSSL_USE_STATIC_LIBS 1)
endif()
include(FindOpenSSL)

if(${OPENSSL_FOUND})
  set(HAVE_OPENSSL 1)
endif()

include(BareosFindLibraryAndHeaders)

bareosfindlibraryandheaders(
  "vixDiskLib" "vixDiskLib.h" "/usr/lib/vmware-vix-disklib-distrib;/usr/lib/vmware-vix-disklib"
)

# check for structmember physicalSectorSize in struct VixDiskLibCreateParams
if(VIXDISKLIB_FOUND)
  include(CheckStructHasMember)
  CHECK_STRUCT_HAS_MEMBER("VixDiskLibCreateParams" physicalSectorSize
    ${VIXDISKLIB_INCLUDE_DIRS}/vixDiskLib.h VIXDISKLIBCREATEPARAMS_HAS_PHYSICALSECTORSIZE)
endif()

if(VIXDISKLIB_FOUND)
  if((NOT DEFINED vmware_server)
     OR (NOT DEFINED vmware_user)
     OR (NOT DEFINED vmware_password)
     OR (NOT DEFINED vmware_datacenter)
     OR (NOT DEFINED vmware_folder)
  )
    string(
      CONCAT
        MSG
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
    set(enable_vmware_test 1 PARENT_SCOPE)
  endif()
elseif(
  (DEFINED vmware_server)
  OR (DEFINED vmware_user)
  OR (DEFINED vmware_password)
  OR (DEFINED vmware_datacenter)
  OR (DEFINED vmware_folder)
)
  message(
    FATAL_ERROR "vmware options were set but VMware Vix Disklib was not found. Cannot run vmware tests."
  )
endif()

bareosfindlibraryandheaders("jansson" "jansson.h" "")
bareosfindlibraryandheaders("rados" "rados/librados.h" "")
bareosfindlibraryandheaders("radosstriper" "radosstriper/libradosstriper.h" "")
bareosfindlibraryandheaders("cephfs" "cephfs/libcephfs.h" "")
bareosfindlibraryandheaders("pthread" "pthread.h" "")
bareosfindlibraryandheaders("cap" "sys/capability.h" "")
bareosfindlibraryandheaders("gfapi" "glusterfs/api/glfs.h" "")
bareosfindlibraryandheaders("droplet" "droplet.h" "")

bareosfindlibraryandheaders("pam" "security/pam_appl.h" "")

bareosfindlibraryandheaders("lzo2" "lzo/lzoconf.h" "")
if(${LZO2_FOUND})
  set(HAVE_LZO 1)
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(LZO2_LIBRARIES "/usr/local/opt/lzo/lib/liblzo2.a")
  endif()
endif()

include(BareosFindLibrary)

bareosfindlibrary("tirpc")
bareosfindlibrary("util")
bareosfindlibrary("dl")
bareosfindlibrary("acl")
# BareosFindLibrary("wrap")
if(NOT ${CMAKE_CXX_COMPILER_ID} MATCHES SunPro)
  bareosfindlibrary("gtest")
  bareosfindlibrary("gtest_main")
  bareosfindlibrary("gmock")
endif()

bareosfindlibrary("pam_wrapper")

if(${HAVE_CAP})
  set(HAVE_LIBCAP 1)
endif()

find_package(ZLIB)
if(${ZLIB_FOUND})
  set(HAVE_LIBZ 1)
endif()

find_package(Readline)
include(thread)
