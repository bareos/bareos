/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
 * daemon.c by Kern Sibbald 2000
 *
 * This code is inspired by the Prentice Hall book
 * "Unix Network Programming" by W. Richard Stevens
 * and later updated from his book "Advanced Programming
 * in the UNIX Environment"
 *
 * Initialize a daemon process completely detaching us from
 * any terminal processes.
 */

#include "include/bareos.h"
#include "lib/berrno.h"
#include "lib/daemon.h"


#if defined(HAVE_WIN32)

void daemon_start() { return; }

#else  // !HAVE_WIN32

#if defined(DEVELOPER)
static void SetupStdFileDescriptors() {}
#else
static void SetupStdFileDescriptors()
{
  extern int debug_level;
  if (debug_level > 0) { return; }
  int fd = open("/dev/null", O_RDONLY);
  ASSERT(fd > STDERR_FILENO);
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  dup2(fd, STDIN_FILENO);
  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);
  close(fd);
}
#endif  // DEVELOPER

static void CloseNonStdFileDescriptors()
{
  constexpr int min_fd_to_be_closed = STDERR_FILENO + 1;

#if defined(HAVE_FCNTL_F_CLOSEM)
  fcntl(min_fd_to_be_closed, F_CLOSEM);
#elif defined(HAVE_CLOSEFROM)
  closefrom(min_fd_to_be_closed);
#else
  for (int i = sysconf(_SC_OPEN_MAX) - 1; i >= min_fd_to_be_closed; i--) {
    close(i);
  }
#endif
}

void daemon_start()
{
  Dmsg0(900, "Enter daemon_start\n");

  switch (fork()) {
    case 0:
      setsid();
      umask(umask(0) | S_IWGRP | S_IROTH | S_IWOTH);
      SetupStdFileDescriptors();
      CloseNonStdFileDescriptors();
      break;
    case -1: {
      BErrNo be;
      Emsg1(M_ABORT, 0, _("Cannot fork to become daemon: ERR=%s\n"),
            be.bstrerror());
      break;
    }
    default:
      exit(0);
  }

  Dmsg0(900, "Exit daemon_start\n");
}
#endif /* defined(HAVE_WIN32) */
