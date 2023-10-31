/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
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


#include <unistd.h>

#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "include/exit_codes.h"
#include "lib/berrno.h"
#include "lib/daemon.h"


#if defined(HAVE_WIN32)

void daemon_start(const char*, int, std::string) { return; }

#else  // !HAVE_WIN32

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

void daemon_start(const char* progname,
                  int pidfile_fd,
                  std::string pidfile_path)
{
  Dmsg0(900, "Enter daemon_start\n");

  switch (fork()) {
    case 0:
      setsid();
      umask(umask(0) | S_IWGRP | S_IROTH | S_IWOTH);

      if (!pidfile_path.empty()) {
        WritePidFile(pidfile_fd, pidfile_path.c_str(), progname);
      }
      SetupStdFileDescriptors();

      break;
    case -1: {
      BErrNo be;
      Emsg1(M_ABORT, 0, T_("Cannot fork to become daemon: ERR=%s\n"),
            be.bstrerror());
      break;
    }
    default:
      exit(BEXIT_SUCCESS);
  }

  Dmsg0(900, "Exit daemon_start\n");
}
#endif /* defined(HAVE_WIN32) */
