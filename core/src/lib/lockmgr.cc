/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2011 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
#include "lib/edit.h"
#include "lib/berrno.h"

#undef ASSERT
#define ASSERT(x)                                                           \
  if (!(x)) {                                                               \
    char* jcr = NULL;                                                       \
    FPmsg3(000, _("ASSERT failed at %s:%i: %s\n"), __FILE__, __LINE__, #x); \
    jcr[0] = 0;                                                             \
  }

#define ASSERT_p(x, f, l)                                      \
  if (!(x)) {                                                  \
    char* jcr = NULL;                                          \
    FPmsg3(000, _("ASSERT failed at %s:%i: %s \n"), f, l, #x); \
    jcr[0] = 0;                                                \
  }

void Lmgr_p(pthread_mutex_t* m)
{
  int errstat;
  if ((errstat = pthread_mutex_lock(m))) {
    BErrNo be;
    e_msg(__FILE__, __LINE__, M_ABORT, 0, _("Mutex lock failure. ERR=%s\n"),
          be.bstrerror(errstat));
  }
}

void Lmgr_v(pthread_mutex_t* m)
{
  int errstat;
  if ((errstat = pthread_mutex_unlock(m))) {
    BErrNo be;
    e_msg(__FILE__, __LINE__, M_ABORT, 0, _("Mutex unlock failure. ERR=%s\n"),
          be.bstrerror(errstat));
  }
}
