/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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
// Testfind Sockets abstraction.

#include "lib/bsock_testfind.h"


#ifndef ENODATA /* not defined on BSD systems */
#  define ENODATA EPIPE
#endif

#ifndef SOL_TCP
#  define SOL_TCP IPPROTO_TCP
#endif

#ifdef HAVE_WIN32
#  define socketRead(fd, buf, len) ::recv(fd, buf, len, 0)
#  define socketWrite(fd, buf, len) ::send(fd, buf, len, 0)
#  define socketClose(fd) ::closesocket(fd)
#else
#  define socketRead(fd, buf, len) ::read(fd, buf, len)
#  define socketWrite(fd, buf, len) ::write(fd, buf, len)
#  define socketClose(fd) ::close(fd)
#endif

BareosSocketTestfind::BareosSocketTestfind() : BareosSocketTCP() {}

BareosSocketTestfind::~BareosSocketTestfind() { destroy(); }

bool BareosSocketTestfind::send() { return true; }
