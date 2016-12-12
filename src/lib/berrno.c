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
 * berrno is a simplistic errno handler that works for
 * Unix, Win32, and BAREOS bpipes.
 *
 * See berrno.h for how to use berrno.
 */

#include "bareos.h"

#ifndef HAVE_WIN32
extern const char *get_signal_name(int sig);
extern int num_execvp_errors;
extern int execvp_errors[];
#endif

const char *berrno::bstrerror()
{
   *m_buf = 0;
#ifdef HAVE_WIN32
   if (m_berrno & b_errno_win32) {
      format_win32_message();
      return (const char *)m_buf;
   }
#else
   int status = 0;

   if (m_berrno & b_errno_exit) {
      status = (m_berrno & ~b_errno_exit);      /* remove bit */
      if (status == 0) {
         return _("Child exited normally.");    /* this really shouldn't happen */
      } else {
         /* Maybe an execvp failure */
         if (status >= 200) {
            if (status < 200 + num_execvp_errors) {
               m_berrno = execvp_errors[status - 200];
            } else {
               return _("Unknown error during program execvp");
            }
         } else {
            Mmsg(m_buf, _("Child exited with code %d"), status);
            return m_buf;
         }
         /* If we drop out here, m_berrno is set to an execvp errno */
      }
   }
   if (m_berrno & b_errno_signal) {
      status = (m_berrno & ~b_errno_signal);        /* remove bit */
      Mmsg(m_buf, _("Child died from signal %d: %s"), status, get_signal_name(status));
      return m_buf;
   }
#endif
   /* Normal errno */
   if (b_strerror(m_berrno, m_buf, 1024) < 0) {
      return _("Invalid errno. No error message possible.");
   }
   return m_buf;
}

void berrno::format_win32_message()
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
   pm_strcpy(m_buf, (const char *)msg);
   LocalFree(msg);
#endif
}
