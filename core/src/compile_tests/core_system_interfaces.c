/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define IGNORE_DEPRECATED_ON      \
  _Pragma("GCC diagnostic push"); \
  _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"");
#define IGNORE_DEPRECATED_OFF _Pragma("GCC diagnostic pop")

void* ptr;
void* ptr2;
void* ptr3;
void* ptr4;

int main(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  // POSIX System Interfaces
  fchmod(0, 0);
  (void)!fchown(0, 0, 0);
  (void)!fchownat(0, ptr, 0, 0, 0);
  fdatasync(0);
  fseeko(ptr, 0, 0);
  futimens(0, ptr);
  int res = getaddrinfo(ptr, ptr2, ptr3, ptr4);
  (void)res;
  glob(ptr, 0, ptr, ptr2);
  inet_ntop(0, ptr, ptr2, 0);
  (void)!lchown(ptr, 0, 0);
  localtime_r(ptr, ptr2);
  nanosleep(ptr, ptr);
  int fd = openat(0, ptr, 0);
  (void)fd;
  poll(ptr, 0, 0);
  IGNORE_DEPRECATED_ON;
  readdir_r(ptr, ptr2, ptr3);  // deprecated
  IGNORE_DEPRECATED_OFF;
  (void)!setreuid(0, 0);
  unlinkat(0, ptr, 0);
  utimes(ptr, ptr2);

  // non-POSIX interfaces
  futimes(0, ptr);         // Linux + BSD
  gethostbyname2(ptr, 0);  // GNU Extension
  (void)getpagesize();  // removed from POSIX, use sysconf(_SC_PAGESIZE) instead
  lutimes(ptr, ptr2);   // Linux + BSD
}
