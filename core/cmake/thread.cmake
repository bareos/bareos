#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2021 Bareos GmbH & Co. KG
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

include(CheckIncludeFiles)
include(CheckCSourceCompiles)
include(CMakePushCheckState)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# pthread_attr_get_np - e.g. on FreeBSD
cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${PTHREAD_LIBRARIES})
check_c_source_compiles(
  "
  #include <pthread.h>
  #if __has_include(<pthread_np.h>)
  #  include <pthread_np.h>
  #endif
  int main() { pthread_attr_t a; pthread_attr_get_np(pthread_self(), &a); }
  "
  HAVE_PTHREAD_ATTR_GET_NP
)
cmake_pop_check_state()
