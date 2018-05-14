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
INCLUDE (CMakePushCheckState)
INCLUDE (CheckSymbolExists)


CHECK_SYMBOL_EXISTS(__stub_lchmod features.h LCHMOD_IS_A_STUB1)
CHECK_SYMBOL_EXISTS(__stub___lchmod  features_h LCHMOD_IS_A_STUB2)



if ("${LCHMOD_IS_A_STUB1}" OR "${LCHMOD_IS_A_STUB2}")
   MESSAGE(STATUS " lchmod is a stub, setting HAVE_LCHMOD to 0")
   set (HAVE_LCHMOD 0)
endif()

CHECK_SYMBOL_EXISTS(poll poll.h HAVE_POLL)
CHECK_SYMBOL_EXISTS(alloca alloca.h HAVE_ALLOCA)

cmake_push_check_state()
SET(CMAKE_REQUIRED_LIBRARIES ${DL_LIBRARIES})
CHECK_SYMBOL_EXISTS(dlerror dlfcn.h HAVE_DLERROR)
cmake_pop_check_state()

CHECK_SYMBOL_EXISTS(va_copy stdarg.h HAVE_VA_COPY)


cmake_push_check_state()
SET(CMAKE_REQUIRED_LIBRARIES ${RADOS_LIBRARIES})
CHECK_SYMBOL_EXISTS(rados_ioctx_set_namespace rados/librados.h  HAVE_RADOS_NAMESPACES)
CHECK_SYMBOL_EXISTS(rados_nobjects_list_open rados/librados.h HAVE_RADOS_NOBJECTS_LIST)
cmake_pop_check_state()

