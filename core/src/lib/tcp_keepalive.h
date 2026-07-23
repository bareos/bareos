/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_TCP_KEEPALIVE_H_
#define BAREOS_LIB_TCP_KEEPALIVE_H_

#include "include/bareos.h"

class JobControlRecord;

struct TcpKeepaliveSettings {
  bool enabled{true};
  utime_t idle_time{60};
  utime_t probe_interval{60};
  int probe_count{3};
};

inline constexpr utime_t kDefaultTcpKeepaliveIdleTime = 60;
inline constexpr utime_t kDefaultTcpKeepaliveProbeInterval = 60;
inline constexpr int kDefaultTcpKeepaliveProbeCount = 3;

TcpKeepaliveSettings ResolveTcpKeepaliveSettings(utime_t heartbeat_interval);
bool ApplyTcpKeepaliveSettings(JobControlRecord* jcr,
                               int sockfd,
                               const TcpKeepaliveSettings& settings,
                               bool log_warnings = true);

#endif  // BAREOS_LIB_TCP_KEEPALIVE_H_
