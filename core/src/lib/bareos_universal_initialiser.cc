/*
   BAREOS® - Backup Archiving REcovery Open Sourced

Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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

#include "bareos_universal_initialiser.h"

#include <stdio.h>
#if defined(HAVE_WIN32)
#  include <winsock2.h>
#  include <windows.h>
#endif

BareosUniversalInitialiser universal_initialiser();

BareosUniversalInitialiser::BareosUniversalInitialiser()
{
#if HAVE_WIN32
  WORD wVersionRequested = MAKEWORD(1, 1);
  WSADATA wsaData;

  int err = WSAStartup(wVersionRequested, &wsaData);

  if (err != 0) {
    printf("Can not start Windows Sockets\n");
    errno = ENOSYS;
  }
#endif
}

BareosUniversalInitialiser::~BareosUniversalInitialiser()
{
#if HAVE_WIN32
  WSACleanup();
#endif
}
