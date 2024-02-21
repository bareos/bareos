/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_FILED_HEARTBEAT_H_
#define BAREOS_FILED_HEARTBEAT_H_

#include <thread>
#include <atomic>
#include <optional>

namespace filedaemon {

class heartbeat_sd_dir {
  std::atomic<bool> stop_requested{false};
  std::thread thread;

 protected:
  void send_heartbeat(BareosSocket* sd, BareosSocket* dir, time_t interval);

 public:
  heartbeat_sd_dir(BareosSocket* sd, BareosSocket* dir, time_t interval);

  heartbeat_sd_dir(const heartbeat_sd_dir&) = delete;
  heartbeat_sd_dir& operator=(const heartbeat_sd_dir&) = delete;
  heartbeat_sd_dir(heartbeat_sd_dir&&) = delete;
  heartbeat_sd_dir& operator=(heartbeat_sd_dir&&) = delete;

  ~heartbeat_sd_dir();
};

class heartbeat_dir {
  std::atomic<bool> stop_requested{false};
  std::thread thread;

 protected:
  void send_heartbeat(BareosSocket* dir, time_t interval);

 public:
  heartbeat_dir(BareosSocket* dir, time_t interval);

  heartbeat_dir(const heartbeat_dir&) = delete;
  heartbeat_dir& operator=(const heartbeat_dir&) = delete;
  heartbeat_dir(heartbeat_dir&&) = delete;
  heartbeat_dir& operator=(heartbeat_dir&&) = delete;

  ~heartbeat_dir();
};

std::optional<heartbeat_sd_dir> MakeHeartbeatMonitor(JobControlRecord* jcr);
std::optional<heartbeat_dir> MakeDirHeartbeat(JobControlRecord* jcr);

} /* namespace filedaemon */
#endif  // BAREOS_FILED_HEARTBEAT_H_
