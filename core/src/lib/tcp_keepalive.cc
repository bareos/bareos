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

#include "lib/tcp_keepalive.h"

#include <algorithm>
#include <limits>

#include "include/jcr.h"
#include "lib/berrno.h"

#ifndef HAVE_WIN32
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#endif

#ifdef HAVE_MSVC
#  include "mstcpip.h"
#endif

#ifndef SOL_TCP
#  define SOL_TCP IPPROTO_TCP
#endif

namespace {
#ifdef HAVE_WIN32
void LogKeepaliveFailure(JobControlRecord* jcr,
                         bool log_warnings,
                         const char* message,
                         int value)
{
  if (jcr && log_warnings) {
    Qmsg2(jcr, M_WARNING, 0, T_(message), value);
  } else {
    Dmsg1(100, message, value);
  }
}
#endif
void LogKeepaliveFailure(JobControlRecord* jcr,
                         bool log_warnings,
                         const char* message,
                         int value,
                         const char* error)
{
  if (jcr && log_warnings) {
    Qmsg3(jcr, M_WARNING, 0, T_(message), value, error);
  } else {
    Dmsg2(100, message, value, error);
  }
}

void LogKeepaliveFailure(JobControlRecord* jcr,
                         bool log_warnings,
                         const char* message,
                         const char* error)
{
  if (jcr && log_warnings) {
    Qmsg2(jcr, M_WARNING, 0, T_(message), error);
  } else {
    Dmsg1(100, message, error);
  }
}

int ClampKeepaliveSeconds(utime_t value)
{
  constexpr auto kMaxUnixKeepaliveSeconds
      = static_cast<utime_t>(std::numeric_limits<int>::max());
#ifdef HAVE_WIN32
  constexpr auto kMaxWindowsKeepaliveSeconds
      = static_cast<utime_t>(std::numeric_limits<u_long>::max() / 1000UL);
  const auto max_keepalive_seconds
      = std::min(kMaxUnixKeepaliveSeconds, kMaxWindowsKeepaliveSeconds);
#else
  const auto max_keepalive_seconds = kMaxUnixKeepaliveSeconds;
#endif

  if (value > max_keepalive_seconds) { value = max_keepalive_seconds; }

  return static_cast<int>(value);
}
}  // namespace

TcpKeepaliveSettings ResolveTcpKeepaliveSettings(utime_t heartbeat_interval)
{
  auto keepalive_time = heartbeat_interval;
  if (!keepalive_time) { keepalive_time = kDefaultTcpKeepaliveIdleTime; }

  return {
      .enabled = true,
      .idle_time = keepalive_time,
      .probe_interval = keepalive_time ? keepalive_time
                                       : kDefaultTcpKeepaliveProbeInterval,
      .probe_count = kDefaultTcpKeepaliveProbeCount,
  };
}

bool ApplyTcpKeepaliveSettings(JobControlRecord* jcr,
                               int sockfd,
                               const TcpKeepaliveSettings& settings,
                               bool log_warnings)
{
  bool ok = true;
  int value = int(settings.enabled);

  if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (sockopt_val_t)&value,
                 sizeof(value))
      < 0) {
    BErrNo be;
    LogKeepaliveFailure(jcr, log_warnings,
                        "Failed to set SO_KEEPALIVE on socket: %s\n",
                        be.bstrerror());
    return false;
  }

  if (!settings.enabled) { return true; }

  const int idle_time = ClampKeepaliveSeconds(settings.idle_time);
  const int probe_interval = ClampKeepaliveSeconds(settings.probe_interval);
  const int probe_count = std::max(settings.probe_count, 1);

#ifdef HAVE_WIN32
  struct s_tcp_keepalive {
    u_long onoff;
    u_long keepalivetime;
    u_long keepaliveinterval;
  } value_to_set;
  value_to_set.onoff = 1;
  value_to_set.keepalivetime = static_cast<u_long>(idle_time) * 1000UL;
  value_to_set.keepaliveinterval = static_cast<u_long>(probe_interval) * 1000UL;
  DWORD got = 0;
  if (WSAIoctl(sockfd, SIO_KEEPALIVE_VALS, &value_to_set, sizeof(value_to_set),
               NULL, 0, &got, NULL, NULL)
      != 0) {
    LogKeepaliveFailure(jcr, log_warnings,
                        "Failed to set SIO_KEEPALIVE_VALS on socket: %d\n",
                        WSAGetLastError());
    ok = false;
  }
#else
#  if defined(TCP_KEEPIDLE)
  if (setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (sockopt_val_t)&idle_time,
                 sizeof(idle_time))
      < 0) {
    BErrNo be;
    LogKeepaliveFailure(jcr, log_warnings,
                        "Failed to set TCP_KEEPIDLE = %d on socket: %s\n",
                        idle_time, be.bstrerror());
    ok = false;
  }
#  elif defined(TCP_KEEPALIVE)
  if (setsockopt(sockfd, SOL_TCP, TCP_KEEPALIVE, (sockopt_val_t)&idle_time,
                 sizeof(idle_time))
      < 0) {
    BErrNo be;
    LogKeepaliveFailure(jcr, log_warnings,
                        "Failed to set TCP_KEEPALIVE = %d on socket: %s\n",
                        idle_time, be.bstrerror());
    ok = false;
  }
#  endif

#  if defined(TCP_KEEPINTVL)
  if (setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL,
                 (sockopt_val_t)&probe_interval, sizeof(probe_interval))
      < 0) {
    BErrNo be;
    LogKeepaliveFailure(jcr, log_warnings,
                        "Failed to set TCP_KEEPINTVL = %d on socket: %s\n",
                        probe_interval, be.bstrerror());
    ok = false;
  }
#  endif

#  if defined(TCP_KEEPCNT)
  if (setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (sockopt_val_t)&probe_count,
                 sizeof(probe_count))
      < 0) {
    BErrNo be;
    LogKeepaliveFailure(jcr, log_warnings,
                        "Failed to set TCP_KEEPCNT = %d on socket: %s\n",
                        probe_count, be.bstrerror());
    ok = false;
  }
#  endif
#endif

  return ok;
}
