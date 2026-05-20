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

#include "gtest/gtest.h"

#include "lib/tcp_keepalive.h"

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef SOL_TCP
#  define SOL_TCP IPPROTO_TCP
#endif

namespace {
class SocketDescriptor {
 public:
  explicit SocketDescriptor(int fd) : fd_(fd) {}
  ~SocketDescriptor()
  {
    if (fd_ >= 0) { close(fd_); }
  }

  int get() const { return fd_; }

 private:
  int fd_;
};
}  // namespace

TEST(tcp_keepalive, resolves_default_intervals)
{
  const auto settings = ResolveTcpKeepaliveSettings(0);

  EXPECT_TRUE(settings.enabled);
  EXPECT_EQ(settings.idle_time, kDefaultTcpKeepaliveIdleTime);
  EXPECT_EQ(settings.probe_interval, kDefaultTcpKeepaliveProbeInterval);
  EXPECT_EQ(settings.probe_count, kDefaultTcpKeepaliveProbeCount);
}

TEST(tcp_keepalive, heartbeat_interval_overrides_default_intervals)
{
  const auto settings = ResolveTcpKeepaliveSettings(120);

  EXPECT_TRUE(settings.enabled);
  EXPECT_EQ(settings.idle_time, 120);
  EXPECT_EQ(settings.probe_interval, 120);
  EXPECT_EQ(settings.probe_count, kDefaultTcpKeepaliveProbeCount);
}

TEST(tcp_keepalive, applies_socket_options)
{
  SocketDescriptor fd(socket(AF_INET, SOCK_STREAM, 0));
  ASSERT_GE(fd.get(), 0);

  const TcpKeepaliveSettings settings{
      .enabled = true, .idle_time = 42, .probe_interval = 21, .probe_count = 4};
  ASSERT_TRUE(ApplyTcpKeepaliveSettings(nullptr, fd.get(), settings));

  int keepalive = 0;
  socklen_t keepalive_size = sizeof(keepalive);
  ASSERT_EQ(getsockopt(fd.get(), SOL_SOCKET, SO_KEEPALIVE, &keepalive,
                       &keepalive_size),
            0);
  EXPECT_EQ(keepalive, 1);

#if defined(TCP_KEEPIDLE)
  int idle_time = 0;
  socklen_t idle_time_size = sizeof(idle_time);
  ASSERT_EQ(
      getsockopt(fd.get(), SOL_TCP, TCP_KEEPIDLE, &idle_time, &idle_time_size),
      0);
  EXPECT_EQ(idle_time, 42);
#elif defined(TCP_KEEPALIVE)
  int idle_time = 0;
  socklen_t idle_time_size = sizeof(idle_time);
  ASSERT_EQ(
      getsockopt(fd.get(), SOL_TCP, TCP_KEEPALIVE, &idle_time, &idle_time_size),
      0);
  EXPECT_EQ(idle_time, 42);
#endif

#if defined(TCP_KEEPINTVL)
  int probe_interval = 0;
  socklen_t probe_interval_size = sizeof(probe_interval);
  ASSERT_EQ(getsockopt(fd.get(), SOL_TCP, TCP_KEEPINTVL, &probe_interval,
                       &probe_interval_size),
            0);
  EXPECT_EQ(probe_interval, 21);
#endif

#if defined(TCP_KEEPCNT)
  int probe_count = 0;
  socklen_t probe_count_size = sizeof(probe_count);
  ASSERT_EQ(
      getsockopt(fd.get(), SOL_TCP, TCP_KEEPCNT, &probe_count, &probe_count_size),
      0);
  EXPECT_EQ(probe_count, 4);
#endif
}
