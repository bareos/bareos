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
include(CMakePushCheckState)
include(CheckSymbolExists)

check_symbol_exists(__stub_lchmod features.h LCHMOD_IS_A_STUB1)
check_symbol_exists(__stub___lchmod features_h LCHMOD_IS_A_STUB2)

if(LCHMOD_IS_A_STUB1 OR LCHMOD_IS_A_STUB2)
  message(STATUS "lchmod is a stub, setting HAVE_LCHMOD to 0")
  set(HAVE_LCHMOD 0)
endif()

check_symbol_exists(__stub_chflags features.h CHFLAGS_IS_A_STUB)
if(CHFLAGS_IS_A_STUB)
  message(STATUS "lchflags is a stub, setting HAVE_CHFLAGS to 0")
  set(HAVE_CHFLAGS 0)
endif()

check_symbol_exists(poll poll.h HAVE_POLL)
