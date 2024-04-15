/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2024 Bareos GmbH & Co. KG

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
/**
 * @file
 * Bi-directional pipe structure
 */

#ifndef BAREOS_LIB_BPIPE_H_
#define BAREOS_LIB_BPIPE_H_

#include "btimers.h"

#ifdef HAVE_WIN32
using ProcessId = HANDLE;
#else
using ProcessId = pid_t;
#endif

class Bpipe {
 public:
  ProcessId worker_pid;
  time_t worker_stime;
  int wait;
  btimer_t* timer_id;
  FILE* rfd;
  FILE* wfd;
};

Bpipe* OpenBpipe(const char* prog,
                 int wait,
                 const char* mode,
                 bool dup_stderr = true);
int CloseWpipe(Bpipe* bpipe);
int CloseBpipe(Bpipe* bpipe);

#endif  // BAREOS_LIB_BPIPE_H_
