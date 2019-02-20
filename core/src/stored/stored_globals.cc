/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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

#include "stored_globals.h"

namespace storagedaemon {

StorageResource* me;
char* configfile;

void* start_heap = nullptr;
bool init_done = false;
uint32_t vol_session_time;

static std::mutex mutex_;
static uint32_t vol_session_id_ = 0;

uint32_t NewVolSessionId()
{
  uint32_t id;

  mutex_.lock();
  vol_session_id_++;
  id = vol_session_id_;
  mutex_.unlock();
  return id;
}

} /* namespace storagedaemon */
