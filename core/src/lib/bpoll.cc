/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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

//  Marco van Wieringen, May 2012

#include "include/bareos.h"

#ifdef HAVE_POLL
#  include <poll.h>
/*
 *   Returns: 1 if data available
 *            0 if timeout
 *           -1 if error
 */
int WaitForReadableFd(int fd, int msec, bool ignore_interupts)
{
  struct pollfd pfds[1];
  int events;

  events = POLLIN;
#  if defined(POLLRDNORM)
  events |= POLLRDNORM;
#  endif
#  if defined(POLLRDBAND)
  events |= POLLRDBAND;
#  endif
#  if defined(POLLPRI)
  events |= POLLPRI;
#  endif

  memset(pfds, 0, sizeof(pfds));
  pfds[0].fd = fd;
  pfds[0].events = events;

  for (;;) {
    switch (poll(pfds, 1, msec)) {
      case 0: /* timeout */
        return 0;
      case -1:
        if (ignore_interupts && (errno == EINTR || errno == EAGAIN)) {
          continue;
        }
        return -1; /* error return */
      default:
        if (pfds[0].revents & events) {
          return 1;
        } else {
          return 0;
        }
    }
  }
}

/*
 *   Returns: 1 if data available
 *            0 if timeout
 *           -1 if error
 */
int WaitForWritableFd(int fd, int msec, bool ignore_interupts)
{
  struct pollfd pfds[1];
  int events;

  events = POLLOUT;
#  if defined(POLLWRNORM)
  events |= POLLWRNORM;
#  endif
#  if defined POLLWRBAND
  events |= POLLWRBAND;
#  endif

  memset(pfds, 0, sizeof(pfds));
  pfds[0].fd = fd;
  pfds[0].events = events;

  for (;;) {
    switch (poll(pfds, 1, msec)) {
      case 0: /* timeout */
        return 0;
      case -1:
        if (ignore_interupts && (errno == EINTR || errno == EAGAIN)) {
          continue;
        }
        return -1; /* error return */
      default:
        if (pfds[0].revents & events) {
          return 1;
        } else {
          return 0;
        }
    }
  }
}
#else
/*
 *   Returns: 1 if data available
 *            0 if timeout
 *           -1 if error
 */
int WaitForReadableFd(int fd, int msec, bool ignore_interupts)
{
  fd_set fdset;
  struct timeval tv;

  tv.tv_sec = msec / 1000;
  tv.tv_usec = (msec % 1000) * 1000;

  for (;;) {
    FD_ZERO(&fdset);
    FD_SET((unsigned)fd, &fdset);
    switch (select(fd + 1, &fdset, NULL, NULL, &tv)) {
      case 0: /* timeout */
        return 0;
      case -1:
        if (ignore_interupts && (errno == EINTR || errno == EAGAIN)) {
          continue;
        }
        return -1; /* error return */
      default:
        return 1;
    }
  }
}

/*
 *   Returns: 1 if data available
 *            0 if timeout
 *           -1 if error
 */
#  if defined(HAVE_WIN32)
int WaitForWritableFd(int, int, bool) { return 1; }
#  else
int WaitForWritableFd(int fd, int msec, bool ignore_interupts)
{
  fd_set fdset;
  struct timeval tv;

  tv.tv_sec = msec / 1000;
  tv.tv_usec = (msec % 1000) * 1000;

  for (;;) {
    FD_ZERO(&fdset);
    FD_SET((unsigned)fd, &fdset);
    switch (select(fd + 1, NULL, &fdset, NULL, &tv)) {
      case 0: /* timeout */
        return 0;
      case -1:
        if (ignore_interupts && (errno == EINTR || errno == EAGAIN)) {
          continue;
        }
        return -1; /* error return */
      default:
        return 1;
    }
  }
}
#  endif
#endif /* HAVE_POLL */
