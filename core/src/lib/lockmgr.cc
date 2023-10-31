/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/


#include "include/bareos.h"
#include "lib/berrno.h"
#include <cstddef>

void lock_mutex_impl(pthread_mutex_t& m, const char* file, std::size_t line)
{
  int errstat;
  if ((errstat = pthread_mutex_lock(&m))) {
    BErrNo be;
    e_msg(file, line, M_ABORT, 0, T_("Mutex lock failure. ERR=%s\n"),
          be.bstrerror(errstat));
  }
}

void unlock_mutex_impl(pthread_mutex_t& m, const char* file, std::size_t line)
{
  int errstat;
  if ((errstat = pthread_mutex_unlock(&m))) {
    BErrNo be;
    e_msg(file, line, M_ABORT, 0, T_("Mutex unlock failure. ERR=%s\n"),
          be.bstrerror(errstat));
  }
}
