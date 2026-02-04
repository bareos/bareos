/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2020 Bareos GmbH & Co. KG

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
#include "include/config.h"
#include <pthread.h>
#if __has_include(<pthread_np.h>)
#  include <pthread_np.h>
#endif
#include "pthread_detach_if_not_detached.h"

namespace directordaemon {
void DetachIfNotDetached(pthread_t thr)
{
#if defined(HAVE_WIN32)
  pthread_detach(thr);
#else
  /* only detach if not yet detached */
  int _detachstate;
  pthread_attr_t _gattr;
#  if defined(HAVE_PTHREAD_ATTR_GET_NP)
  pthread_attr_init(&_gattr);
  pthread_attr_get_np(thr, &_gattr);
#  else
  pthread_getattr_np(thr, &_gattr);
#  endif  // defined(HAVE_PTHREAD_ATTR_GET_NP)
  pthread_attr_getdetachstate(&_gattr, &_detachstate);
  pthread_attr_destroy(&_gattr);
  if (_detachstate != PTHREAD_CREATE_DETACHED) { pthread_detach(thr); }
#endif    // defined(HAVE_WIN32)
}
}  // namespace directordaemon
