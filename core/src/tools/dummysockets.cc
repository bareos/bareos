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

#include "tools/dummysockets.h"
#include "filed/fileset.h"

EmptySocket::EmptySocket() : BareosSocketTCP() {}

EmptySocket::~EmptySocket() { destroy(); }

bool EmptySocket::send() { return true; }

DummyFdFilesetSocket::DummyFdFilesetSocket() : BareosSocketTCP() {}

DummyFdFilesetSocket::~DummyFdFilesetSocket() { destroy(); }

bool DummyFdFilesetSocket::send()
{
  StripTrailingJunk(msg);
  filedaemon::AddFileset(jcr, msg);
  return true;
}
