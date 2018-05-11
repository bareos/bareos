/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, July MMIV
 */
/**
 * @file
 * BAREOS errno handler
 *
 * BErrNo is a simplistic errno handler that works for
 * Unix, Win32, and BAREOS bpipes.
 *
 * See BErrNo.h for how to use BErrNo.
 */

#include "include/bareos.h"
#include "lib/bsys.h"

#ifndef HAVE_WIN32
extern const char *get_signal_name(int sig);
extern int num_execvp_errors;
extern int execvp_errors[];
#endif

const char *BErrNo::bstrerror()
{
   *buf_ = 0;
#ifdef HAVE_WIN32
   if (berrno_ & b_errno_win32) {
      FormatWin32Message();
      return (const char *)buf_;
   }
#else
   int status = 0;

   if (berrno_ & b_errno_exit) {
      status = (berrno_ & ~b_errno_exit);      /* remove bit */
      if (status == 0) {
         return _("Child exited normally.");    /* this really shouldn't happen */
      } else {
         /* Maybe an execvp failure */
         if (status >= 200) {
            if (status < 200 + num_execvp_errors) {
               berrno_ = execvp_errors[status - 200];
            } else {
               return _("Unknown error during program execvp");
            }
         } else {
            Mmsg(buf_, _("Child exited with code %d"), status);
            return buf_;
         }
         /* If we drop out here, berrno_ is set to an execvp errno */
      }
   }
   if (berrno_ & b_errno_signal) {
      status = (berrno_ & ~b_errno_signal);        /* remove bit */
      Mmsg(buf_, _("Child died from signal %d: %s"), status, get_signal_name(status));
      return buf_;
   }
#endif
   /* Normal errno */
   if (b_strerror(berrno_, buf_, 1024) < 0) {
      return _("Invalid errno. No error message possible.");
   }
   return buf_;
}

void BErrNo::FormatWin32Message()
{
#ifdef HAVE_WIN32
   LPVOID msg;
   FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
       FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
       NULL,
       GetLastError(),
       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
       (LPTSTR)&msg,
       0,
       NULL);
   PmStrcpy(buf_, (const char *)msg);
   LocalFree(msg);
#endif
}
