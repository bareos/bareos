/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2023 Bareos GmbH & Co. KG

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

#ifndef BAREOS_TOOLS_DUMMYSOCKETS_H_
#define BAREOS_TOOLS_DUMMYSOCKETS_H_

#include "lib/bsock_tcp.h"
#include "include/jcr.h"

class EmptySocket : public BareosSocketTCP {
 public:
  EmptySocket();
  ~EmptySocket();

  bool send() override;
};

class DummyFdFilesetSocket : public BareosSocketTCP {
 public:
  DummyFdFilesetSocket();
  ~DummyFdFilesetSocket();

  JobControlRecord* jcr;

  bool send() override;
};

#endif  // BAREOS_TOOLS_DUMMYSOCKETS_H_
