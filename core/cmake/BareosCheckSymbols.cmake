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
include(CMakePushCheckState)
include(CheckSymbolExists)

check_symbol_exists(__stub_lchmod features.h LCHMOD_IS_A_STUB1)
check_symbol_exists(__stub___lchmod features_h LCHMOD_IS_A_STUB2)

if("${LCHMOD_IS_A_STUB1}" OR "${LCHMOD_IS_A_STUB2}")
  message(STATUS " lchmod is a stub, setting HAVE_LCHMOD to 0")
  set(HAVE_LCHMOD 0)
endif()

check_symbol_exists(poll poll.h HAVE_POLL)
check_symbol_exists(alloca alloca.h HAVE_ALLOCA)

cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${DL_LIBRARIES})
check_symbol_exists(dlerror dlfcn.h HAVE_DLERROR)
cmake_pop_check_state()

check_symbol_exists(va_copy stdarg.h HAVE_VA_COPY)

cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${RADOS_LIBRARIES})
check_symbol_exists(rados_ioctx_set_namespace rados/librados.h
                    HAVE_RADOS_NAMESPACES)
check_symbol_exists(rados_nobjects_list_open rados/librados.h
                    HAVE_RADOS_NOBJECTS_LIST)
cmake_pop_check_state()

if(HAVE_GLUSTERFS_API_GLFS_H)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_LIBRARIES ${GFAPI_LIBRARIES})
  check_cxx_source_compiles("
#include <glusterfs/api/glfs.h>
int main(void)
{
       /* new glfs_ftruncate() passes two additional args */
       return glfs_ftruncate(NULL, 0, NULL, NULL);
}
" GLFS_FTRUNCATE_HAS_FOUR_ARGS)

endif()
cmake_pop_check_state()
